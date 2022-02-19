//////////////////////////////////////////////////////////////////////////////////////
//    akashi - a server for Attorney Online 2                                       //
//    Copyright (C) 2020  scatterflower                                             //
//                                                                                  //
//    This program is free software: you can redistribute it and/or modify          //
//    it under the terms of the GNU Affero General Public License as                //
//    published by the Free Software Foundation, either version 3 of the            //
//    License, or (at your option) any later version.                               //
//                                                                                  //
//    This program is distributed in the hope that it will be useful,               //
//    but WITHOUT ANY WARRANTY; without even the implied warranty of                //
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                 //
//    GNU Affero General Public License for more details.                           //
//                                                                                  //
//    You should have received a copy of the GNU Affero General Public License      //
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.        //
//////////////////////////////////////////////////////////////////////////////////////
#include "include/aoclient.h"

void AOClient::clientData()
{
    if (last_read + m_socket->bytesAvailable() > 30720) { // Client can send a max of 30KB to the server over two sequential reads
        m_socket->close();
    }

    if (last_read == 0) { // i.e. this is the first packet we've been sent
        if (!m_socket->waitForConnected(1000)) {
            m_socket->close();
        }
    }
    QString l_data = QString::fromUtf8(m_socket->readAll());
    last_read = l_data.size();

    if (is_partial) {
        l_data = partial_packet + l_data;
    }
    if (!l_data.endsWith("%")) {
        is_partial = true;
    }

    QStringList l_all_packets = l_data.split("%");
    l_all_packets.removeLast(); // Remove the entry after the last delimiter

    for (const QString &l_single_packet : qAsConst(l_all_packets)) {
        AOPacket l_packet(l_single_packet);
        handlePacket(l_packet);
    }
}

void AOClient::clientDisconnected()
{
#ifdef NET_DEBUG
    qDebug() << remote_ip.toString() << "disconnected";
#endif
    if (m_joined) {
        server->m_player_count--;
        emit server->updatePlayerCount(server->m_player_count);
        server->m_areas[m_current_area]->clientLeftArea(server->getCharID(m_current_char), m_id);
        arup(ARUPType::PLAYER_COUNT, true);
    }

    if (m_current_char != "") {
        server->updateCharsTaken(server->m_areas[m_current_area]);
    }

    bool l_updateLocks = false;

    for (AreaData* l_area : qAsConst(server->m_areas)) {
        l_updateLocks = l_updateLocks || l_area->removeOwner(m_id);
    }

    if (l_updateLocks)
        arup(ARUPType::LOCKED, true);
    arup(ARUPType::CM, true);

    emit clientSuccessfullyDisconnected(m_id);
}

void AOClient::handlePacket(AOPacket packet)
{
#ifdef NET_DEBUG
    qDebug() << "Received packet:" << packet.header << ":" << packet.contents << "args length:" << packet.contents.length();
#endif
    AreaData* l_area = server->m_areas[m_current_area];
    PacketInfo l_info = packets.value(packet.header, {false, 0, &AOClient::pktDefault});

    if (packet.contents.join("").size() > 16384) {
        return;
    }

    if (!checkAuth(l_info.acl_mask)) {
        return;
    }

    if (packet.header != "CH") {
        if (m_is_afk)
            sendServerMessage("You are no longer AFK.");
        m_is_afk = false;
        m_afk_timer->start(ConfigManager::afkTimeout() * 1000);
    }

    if (packet.contents.length() < l_info.minArgs) {
#ifdef NET_DEBUG
        qDebug() << "Invalid packet args length. Minimum is" << info.minArgs << "but only" << packet.contents.length() << "were given.";
#endif
        return;
    }

    (this->*(l_info.action))(l_area, packet.contents.length(), packet.contents, packet);
}

void AOClient::changeArea(int new_area)
{
    if (m_current_area == new_area) {
        sendServerMessage("You are already in area " + server->m_area_names[m_current_area]);
        return;
    }
    if (server->m_areas[new_area]->lockStatus() == AreaData::LockStatus::LOCKED && !server->m_areas[new_area]->invited().contains(m_id) && !checkAuth(ACLFlags.value("BYPASS_LOCKS"))) {
        sendServerMessage("Area " + server->m_area_names[new_area] + " is locked.");
        return;
    }

    if (m_current_char != "") {
        server->m_areas[m_current_area]->changeCharacter(server->getCharID(m_current_char), -1);
        server->updateCharsTaken(server->m_areas[m_current_area]);
    }
    server->m_areas[m_current_area]->clientLeftArea(m_char_id, m_id);
    bool l_character_taken = false;
    if (server->m_areas[new_area]->charactersTaken().contains(server->getCharID(m_current_char))) {
        m_current_char = "";
        m_char_id = -1;
        l_character_taken = true;
    }
    server->m_areas[new_area]->clientJoinedArea(m_char_id, m_id);
    m_current_area = new_area;
    arup(ARUPType::PLAYER_COUNT, true);
    sendEvidenceList(server->m_areas[new_area]);
    sendPacket("HP", {"1", QString::number(server->m_areas[new_area]->defHP())});
    sendPacket("HP", {"2", QString::number(server->m_areas[new_area]->proHP())});
    sendPacket("BN", {server->m_areas[new_area]->background()});
    if (l_character_taken) {
        sendPacket("DONE");
    }
    const QList<QTimer*> l_timers = server->m_areas[m_current_area]->timers();
    for (QTimer* l_timer : l_timers) {
        int l_timer_id = server->m_areas[m_current_area]->timers().indexOf(l_timer) + 1;
        if (l_timer->isActive()) {
            sendPacket("TI", {QString::number(l_timer_id), "2"});
            sendPacket("TI", {QString::number(l_timer_id), "0", QString::number(QTime(0,0).msecsTo(QTime(0,0).addMSecs(l_timer->remainingTime())))});
        }
        else {
            sendPacket("TI", {QString::number(l_timer_id), "3"});
        }
    }
    sendServerMessage("You moved to area " + server->m_area_names[m_current_area]);
    if (server->m_areas[m_current_area]->sendAreaMessageOnJoin())
        sendServerMessage(server->m_areas[m_current_area]->areaMessage());

    if (server->m_areas[m_current_area]->lockStatus() == AreaData::LockStatus::SPECTATABLE)
        sendServerMessage("Area " + server->m_area_names[m_current_area] + " is spectate-only; to chat IC you will need to be invited by the CM.");
}

bool AOClient::changeCharacter(int char_id)
{
    AreaData* l_area = server->m_areas[m_current_area];

    if(char_id >= server->m_characters.length())
        return false;

    if (m_is_charcursed && !m_charcurse_list.contains(char_id)) {
        return false;
    }
    
    bool l_successfulChange = l_area->changeCharacter(server->getCharID(m_current_char), char_id);

    if (char_id < 0) {
        m_current_char = "";
    }

    if (l_successfulChange == true) {
        QString l_char_selected = server->m_characters[char_id];
        m_current_char = l_char_selected;
        m_pos = "";
        server->updateCharsTaken(l_area);
        sendPacket("PV", {QString::number(m_id), "CID", QString::number(char_id)});
        return true;
    }
    return false;
}

void AOClient::changePosition(QString new_pos)
{
    m_pos = new_pos;
    sendServerMessage("Position changed to " + m_pos + ".");
    sendPacket("SP", {m_pos});
}

void AOClient::handleCommand(QString command, int argc, QStringList argv)
{
    CommandInfo l_info = commands.value(command, {false, -1, &AOClient::cmdDefault});

    if (!checkAuth(l_info.acl_mask)) {
        sendServerMessage("You do not have permission to use that command.");
        return;
    }

    if (argc < l_info.minArgs) {
        sendServerMessage("Invalid command syntax.");
        sendServerMessage("The expected syntax for this command is: \n" + ConfigManager::commandHelp(command).usage);
        return;
    }

    (this->*(l_info.action))(argc, argv);
}

void AOClient::arup(ARUPType type, bool broadcast)
{
    QStringList l_arup_data;
    l_arup_data.append(QString::number(type));
    for (AreaData* l_area : qAsConst(server->m_areas)) {
        switch(type) {
            case ARUPType::PLAYER_COUNT: {
                l_arup_data.append(QString::number(l_area->playerCount()));
                break;
            }
            case ARUPType::STATUS: {
                QString l_area_status = QVariant::fromValue(l_area->status()).toString().replace("_", "-"); // LOOKING_FOR_PLAYERS to LOOKING-FOR-PLAYERS
                l_arup_data.append(l_area_status);
                break;
            }
            case ARUPType::CM: {
                if (l_area->owners().isEmpty())
                    l_arup_data.append("FREE");
                else {
                    QStringList l_area_owners;
                    const QList<int> l_owner_ids = l_area->owners();
                    for (int l_owner_id : l_owner_ids) {
                        AOClient* l_owner = server->getClientByID(l_owner_id);
                        l_area_owners.append("[" + QString::number(l_owner->m_id) + "] " + l_owner->m_current_char);
                    }
                    l_arup_data.append(l_area_owners.join(", "));
                }
                break;
            }
            case ARUPType::LOCKED: {
                QString l_lock_status = QVariant::fromValue(l_area->lockStatus()).toString();
                l_arup_data.append(l_lock_status);
                break;
            }
            default: {
                return;
            }
        }
    }
    if (broadcast)
        server->broadcast(AOPacket("ARUP", l_arup_data));
    else
        sendPacket("ARUP", l_arup_data);
}

void AOClient::fullArup() {
    arup(ARUPType::PLAYER_COUNT, false);
    arup(ARUPType::STATUS, false);
    arup(ARUPType::CM, false);
    arup(ARUPType::LOCKED, false);
}

void AOClient::sendPacket(AOPacket packet)
{
#ifdef NET_DEBUG
    qDebug() << "Sent packet:" << packet.header << ":" << packet.contents;
#endif
    packet.contents.replaceInStrings("#", "<num>")
                   .replaceInStrings("%", "<percent>")
                   .replaceInStrings("$", "<dollar>");
    if (packet.header != "LE")
        packet.contents.replaceInStrings("&", "<and>");
    m_socket->write(packet.toUtf8());
    m_socket->flush();
}

void AOClient::sendPacket(QString header, QStringList contents)
{
    sendPacket(AOPacket(header, contents));
}

void AOClient::sendPacket(QString header)
{
    sendPacket(AOPacket(header, {}));
}

void AOClient::calculateIpid()
{
    // TODO: add support for longer ipids?
    // This reduces the (fairly high) chance of
    // birthday paradox issues arising. However,
    // typing more than 8 characters might be a
    // bit cumbersome.

    QCryptographicHash hash(QCryptographicHash::Md5); // Don't need security, just hashing for uniqueness

    hash.addData(m_remote_ip.toString().toUtf8());

    m_ipid = hash.result().toHex().right(8); // Use the last 8 characters (4 bytes)
}

void AOClient::sendServerMessage(QString message)
{
    sendPacket("CT", {ConfigManager::serverName(), message, "1"});
}

void AOClient::sendServerMessageArea(QString message)
{
    server->broadcast(AOPacket("CT", {ConfigManager::serverName(), message, "1"}), m_current_area);
}

void AOClient::sendServerBroadcast(QString message)
{
    server->broadcast(AOPacket("CT", {ConfigManager::serverName(), message, "1"}));
}

bool AOClient::checkAuth(unsigned long long acl_mask)
{
#ifdef SKIP_AUTH
    return true;
#endif
    if (acl_mask != ACLFlags.value("NONE")) {
        if (acl_mask == ACLFlags.value("CM")) {
            AreaData* l_area = server->m_areas[m_current_area];
            if (l_area->owners().contains(m_id))
                return true;
        }
        else if (!m_authenticated) {
            return false;
        }
        switch (ConfigManager::authType()) {
        case DataTypes::AuthType::SIMPLE:
            return m_authenticated;
            break;
        case DataTypes::AuthType::ADVANCED:
            unsigned long long l_user_acl = server->db_manager->getACL(m_moderator_name);
            return (l_user_acl & acl_mask) != 0;
            break;
        }
    }
    return true;
}


QString AOClient::getIpid() const
{
    return m_ipid;
}

QString AOClient::getHwid() const
{
    return m_hwid;
}

Server* AOClient::getServer() { return server; }

void AOClient::onAfkTimeout()
{
    if (!m_is_afk)
        sendServerMessage("You are now AFK.");
    m_is_afk = true;
}

AOClient::AOClient(Server *p_server, QTcpSocket *p_socket, QObject *parent, int user_id, MusicManager *p_manager)
    : QObject(parent),
      m_id(user_id),
      m_remote_ip(p_socket->peerAddress()),
      m_password(""),
      m_joined(false),
      m_current_area(0),
      m_current_char(""),
      m_socket(p_socket),
      server(p_server),
      is_partial(false),
      m_last_wtce_time(0),
      m_music_manager(p_manager)
{
    m_afk_timer = new QTimer;
    m_afk_timer->setSingleShot(true);
    connect(m_afk_timer, &QTimer::timeout,
            this, &AOClient::onAfkTimeout);
}

AOClient::~AOClient() {
    m_socket->deleteLater();
}
