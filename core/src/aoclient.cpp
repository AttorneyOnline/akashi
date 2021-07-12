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
    if (last_read + socket->bytesAvailable() > 30720) { // Client can send a max of 30KB to the server over two sequential reads
        socket->close();
    }

    QString data = QString::fromUtf8(socket->readAll());
    last_read = data.size();

    if (is_partial) {
        data = partial_packet + data;
    }
    if (!data.endsWith("%")) {
        is_partial = true;
    }

    QStringList all_packets = data.split("%");
    all_packets.removeLast(); // Remove the entry after the last delimiter

    for (QString single_packet : all_packets) {
        //AOPacket packet(single_packet);
        AOPacket* packet = PacketFactory::createPacket(single_packet);
        if (!packet) {
            qDebug() << "Unimplemented packet: " << single_packet;
            continue;
        }
        handlePacket(packet);
    }
}

void AOClient::clientDisconnected()
{
#ifdef NET_DEBUG
    qDebug() << remote_ip.toString() << "disconnected";
#endif
    if (joined) {
        server->player_count--;
        server->areas[current_area]->clientLeftArea(server->getCharID(current_char));
        arup(ARUPType::PLAYER_COUNT, true);
    }

    if (current_char != "") {
        server->updateCharsTaken(server->areas[current_area]);
    }

    bool l_updateLocks = false;

    for (AreaData* area : server->areas) {
        l_updateLocks = l_updateLocks || area->removeOwner(id);
    }

    if (l_updateLocks)
        arup(ARUPType::LOCKED, true);
    arup(ARUPType::CM, true);
}

void AOClient::handlePacket(AOPacket* packet)
{
#ifdef NET_DEBUG
    qDebug() << "Received packet:" << packet.header << ":" << packet.contents << "args length:" << packet.contents.length();
#endif
    AreaData* area = server->areas[current_area];

    if (packet->getContents().join("").size() > 16384) {
        return;
    }

    if (!checkAuth(packet->getAclMask())) {
        return;
    }

    if (packet->getHeader() != "CH") {
        if (is_afk)
            sendServerMessage("You are no longer AFK.");
        is_afk = false;
        afk_timer->start(ConfigManager::afkTimeout() * 1000);
    }

    if (packet->getContents().length() < packet->getMinArgs()) {
#ifdef NET_DEBUG
        qDebug() << "Invalid packet args length. Minimum is" << info.minArgs << "but only" << packet.contents.length() << "were given.";
#endif
        return;
    }

    packet->handlePacket(area, *this);
}

void AOClient::changeArea(int new_area)
{
    if (current_area == new_area) {
        sendServerMessage("You are already in area " + server->area_names[current_area]);
        return;
    }
    if (server->areas[new_area]->lockStatus() == AreaData::LockStatus::LOCKED && !server->areas[new_area]->invited().contains(id) && !checkAuth(ACLFlags.value("BYPASS_LOCKS"))) {
        sendServerMessage("Area " + server->area_names[new_area] + " is locked.");
        return;
    }

    if (current_char != "") {
        server->areas[current_area]->changeCharacter(server->getCharID(current_char), -1);
        server->updateCharsTaken(server->areas[current_area]);
    }
    server->areas[current_area]->clientLeftArea(char_id);
    bool character_taken = false;
    if (server->areas[new_area]->charactersTaken().contains(server->getCharID(current_char))) {
        current_char = "";
        char_id = -1;
        character_taken = true;
    }
    server->areas[new_area]->clientJoinedArea(char_id);
    current_area = new_area;
    arup(ARUPType::PLAYER_COUNT, true);
    sendEvidenceList(server->areas[new_area]);
    sendPacket("HP", {"1", QString::number(server->areas[new_area]->defHP())});
    sendPacket("HP", {"2", QString::number(server->areas[new_area]->proHP())});
    sendPacket("BN", {server->areas[new_area]->background()});
    if (character_taken) {
        sendPacket("DONE");
    }
    for (QTimer* timer : server->areas[current_area]->timers()) {
        int timer_id = server->areas[current_area]->timers().indexOf(timer) + 1;
        if (timer->isActive()) {
            sendPacket("TI", {QString::number(timer_id), "2"});
            sendPacket("TI", {QString::number(timer_id), "0", QString::number(QTime(0,0).msecsTo(QTime(0,0).addMSecs(timer->remainingTime())))});
        }
        else {
            sendPacket("TI", {QString::number(timer_id), "3"});
        }
    }
    sendServerMessage("You moved to area " + server->area_names[current_area]);
    if (server->areas[current_area]->lockStatus() == AreaData::LockStatus::SPECTATABLE)
        sendServerMessage("Area " + server->area_names[current_area] + " is spectate-only; to chat IC you will need to be invited by the CM.");
}

bool AOClient::changeCharacter(int char_id)
{
    AreaData* area = server->areas[current_area];

    if(char_id >= server->characters.length())
        return false;

    if (is_charcursed && !charcurse_list.contains(char_id)) {
        return false;
    }
    
    bool l_successfulChange = area->changeCharacter(server->getCharID(current_char), char_id);

    if (char_id < 0) {
        current_char = "";
    }

    if (l_successfulChange == true) {
        QString char_selected = server->characters[char_id];
        current_char = char_selected;
        pos = "";
        server->updateCharsTaken(area);
        sendPacket("PV", {QString::number(id), "CID", QString::number(char_id)});
        return true;
    }
    return false;
}

void AOClient::changePosition(QString new_pos)
{
    pos = new_pos;
    sendServerMessage("Position changed to " + pos + ".");
    sendPacket("SP", {pos});
}

void AOClient::handleCommand(QString command, int argc, QStringList argv)
{
    CommandInfo info = commands.value(command, {false, -1, &AOClient::cmdDefault});

    if (!checkAuth(info.acl_mask)) {
        sendServerMessage("You do not have permission to use that command.");
        return;
    }

    if (argc < info.minArgs) {
        sendServerMessage("Invalid command syntax.");
        return;
    }

    (this->*(info.action))(argc, argv);
}

void AOClient::arup(ARUPType type, bool broadcast)
{
    QStringList arup_data;
    arup_data.append(QString::number(type));
    for (AreaData* area : server->areas) {
        switch(type) {
            case ARUPType::PLAYER_COUNT: {
                arup_data.append(QString::number(area->playerCount()));
                break;
            }
            case ARUPType::STATUS: {
                QString area_status = QVariant::fromValue(area->status()).toString().replace("_", "-"); // LOOKING_FOR_PLAYERS to LOOKING-FOR-PLAYERS
                arup_data.append(area_status);
                break;
            }
            case ARUPType::CM: {
                if (area->owners().isEmpty())
                    arup_data.append("FREE");
                else {
                    QStringList area_owners;
                    for (int owner_id : area->owners()) {
                        AOClient* owner = server->getClientByID(owner_id);
                        area_owners.append("[" + QString::number(owner->id) + "] " + owner->current_char);
                    }
                    arup_data.append(area_owners.join(", "));
                }
                break;
            }
            case ARUPType::LOCKED: {
                QString lock_status = QVariant::fromValue(area->lockStatus()).toString();
                arup_data.append(lock_status);
                break;
            }
            default: {
                return;
            }
        }
    }
    if (broadcast)
        server->broadcast(*PacketFactory::createPacket("ARUP", arup_data));
    else
        sendPacket("ARUP", arup_data);
}

void AOClient::fullArup() {
    arup(ARUPType::PLAYER_COUNT, false);
    arup(ARUPType::STATUS, false);
    arup(ARUPType::CM, false);
    arup(ARUPType::LOCKED, false);
}

void AOClient::sendPacket(AOPacket& packet)
{
#ifdef NET_DEBUG
    qDebug() << "Sent packet:" << packet.header << ":" << packet.contents;
#endif
    //packet.contents.replaceInStrings("#", "<num>")
    //               .replaceInStrings("%", "<percent>")
    //               .replaceInStrings("$", "<dollar>");
    //if (packet.header != "LE")
    //    packet.contents.replaceInStrings("&", "<and>");
    // Make a packet member function to do the above
    socket->write(packet.toUtf8());
    socket->flush();
}

void AOClient::sendPacket(QString header, QStringList contents)
{
    sendPacket(*PacketFactory::createPacket(header, contents));
}

void AOClient::sendPacket(QString header)
{
    sendPacket(*PacketFactory::createPacket(header, {}));
}

void AOClient::calculateIpid()
{
    // TODO: add support for longer ipids?
    // This reduces the (fairly high) chance of
    // birthday paradox issues arising. However,
    // typing more than 8 characters might be a
    // bit cumbersome.

    QCryptographicHash hash(QCryptographicHash::Md5); // Don't need security, just hashing for uniqueness

    hash.addData(remote_ip.toString().toUtf8());

    ipid = hash.result().toHex().right(8); // Use the last 8 characters (4 bytes)
}

void AOClient::sendServerMessage(QString message)
{
    sendPacket("CT", {ConfigManager::serverName(), message, "1"});
}

void AOClient::sendServerMessageArea(QString message)
{
    server->broadcast(*PacketFactory::createPacket("CT", {ConfigManager::serverName(), message, "1"}), current_area);
}

void AOClient::sendServerBroadcast(QString message)
{
    server->broadcast(*PacketFactory::createPacket("CT", {ConfigManager::serverName(), message, "1"}));
}

bool AOClient::checkAuth(unsigned long long acl_mask)
{
#ifdef SKIP_AUTH
    return true;
#endif
    if (acl_mask != ACLFlags.value("NONE")) {
        if (acl_mask == ACLFlags.value("CM")) {
            AreaData* area = server->areas[current_area];
            if (area->owners().contains(id))
                return true;
        }
        else if (!authenticated) {
            return false;
        }
        switch (ConfigManager::authType()) {
        case DataTypes::AuthType::SIMPLE:
            return authenticated;
            break;
        case DataTypes::AuthType::ADVANCED:
            unsigned long long user_acl = server->db_manager->getACL(moderator_name);
            return (user_acl & acl_mask) != 0;
            break;
        }
    }
    return true;
}


QString AOClient::getIpid() const { return ipid; }

Server* AOClient::getServer() { return server; }

void AOClient::onAfkTimeout()
{
    if (!is_afk)
        sendServerMessage("You are now AFK.");
    is_afk = true;
}

AOClient::~AOClient() {
    socket->deleteLater();
}
