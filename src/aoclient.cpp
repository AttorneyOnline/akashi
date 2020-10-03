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

AOClient::AOClient(Server* p_server, QTcpSocket* p_socket, QObject* parent)
    : QObject(parent)
{
    socket = p_socket;
    server = p_server;
    joined = false;
    password = "";
    current_area = 0;
    current_char = "";
    remote_ip = p_socket->peerAddress();
    is_partial = false;
    last_wtce_time = 0;
    last_message = "";
}

void AOClient::clientData()
{
    QString data = QString::fromUtf8(socket->readAll());
    // qDebug() << "From" << remote_ip << ":" << data;

    if (is_partial) {
        data = partial_packet + data;
    }
    if (!data.endsWith("%")) {
        is_partial = true;
    }

    QStringList all_packets = data.split("%");
    all_packets.removeLast(); // Remove the entry after the last delimiter

    for (QString single_packet : all_packets) {
        AOPacket packet(single_packet);
        handlePacket(packet);
    }
}

void AOClient::clientDisconnected()
{
    qDebug() << remote_ip.toString() << "disconnected";
    if (joined) {
        server->player_count--;
        server->areas[current_area]->player_count--;
        arup(ARUPType::PLAYER_COUNT, true);
    }
    if (current_char != "") {
        server->areas[current_area]->characters_taken[current_char] =
            false;
        server->updateCharsTaken(server->areas[current_area]);
    }
}

void AOClient::handlePacket(AOPacket packet)
{
    qDebug() << "Received packet:" << packet.header << ":" << packet.contents << "args length:" << packet.contents.length();
    AreaData* area = server->areas[current_area];
    PacketInfo info = packets.value(packet.header, {false, 0, &AOClient::pktDefault});

    if (!checkAuth(info.acl_mask)) {
        qDebug() << "Unauthenticated client" << getIpid() << "attempted to use privileged packet" << packet.header;
        return;
    }

    if (packet.contents.length() < info.minArgs) {
        qDebug() << "Invalid packet args length. Minimum is" << info.minArgs << "but only" << packet.contents.length() << "were given.";
        return;
    }

    (this->*(info.action))(area, packet.contents.length(), packet.contents, packet);
}

void AOClient::changeArea(int new_area)
{
    // TODO: function to send chat messages with hostname automatically
    if (current_area == new_area) {
        sendServerMessage("You are already in area " + server->area_names[current_area]);
        return;
    }
    if (server->areas[new_area]->locked) {
        sendServerMessage("Area " + server->area_names[new_area] + " is locked.");
        return;
    }
    if (current_char != "") {
        server->areas[current_area]->characters_taken[current_char] =
            false;
        server->updateCharsTaken(server->areas[current_area]);
    }
    server->areas[new_area]->player_count++;
    server->areas[current_area]->player_count--;
    current_area = new_area;
    arup(ARUPType::PLAYER_COUNT, true);
    // send le
    sendPacket("HP", {"1", QString::number(server->areas[new_area]->def_hp)});
    sendPacket("HP", {"2", QString::number(server->areas[new_area]->pro_hp)});
    sendPacket("BN", {server->areas[new_area]->background});
    if (server->areas[current_area]->characters_taken[current_char]) {
        server->updateCharsTaken(server->areas[current_area]);
        current_char = "";
        sendPacket("DONE");
    }
    else {
        server->areas[current_area]->characters_taken[current_char] = true;
        server->updateCharsTaken(server->areas[current_area]);
    }
    sendServerMessage("You have been moved to area " + server->area_names[current_area]);
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
        if (type == ARUPType::PLAYER_COUNT) {
            arup_data.append(QString::number(area->player_count));
        }
        else if (type == ARUPType::STATUS) {
            arup_data.append(area->status);
        }
        else if (type == ARUPType::CM) {
            arup_data.append(area->current_cm);
        }
        else if (type == ARUPType::LOCKED) {
            arup_data.append(area->locked ? "LOCKED" : "FREE");
        }
        else return;
    }
    if (broadcast)
        server->broadcast(AOPacket("ARUP", arup_data));
    else
        sendPacket("ARUP", arup_data);
}

void AOClient::fullArup() {
    arup(ARUPType::PLAYER_COUNT, false);
    arup(ARUPType::STATUS, false);
    arup(ARUPType::CM, false);
    arup(ARUPType::LOCKED, false);
}

void AOClient::sendPacket(AOPacket packet)
{
     qDebug() << "Sent packet:" << packet.header << ":" << packet.contents;
    socket->write(packet.toUtf8());
    socket->flush();
}

void AOClient::sendPacket(QString header, QStringList contents)
{
    sendPacket(AOPacket(header, contents));
}

void AOClient::sendPacket(QString header)
{
    sendPacket(AOPacket(header, {}));
}

QString AOClient::getHwid() { return hwid; }

void AOClient::setHwid(QString p_hwid)
{
    // TODO: add support for longer hwids?
    // This reduces the (fairly high) chance of
    // birthday paradox issues arising. However,
    // typing more than 8 characters might be a
    // bit cumbersome.
    hwid = p_hwid;

    QCryptographicHash hash(
        QCryptographicHash::Md5); // Don't need security, just
                                  // hashing for uniqueness
    QString concat_ip_id = remote_ip.toString() + p_hwid;
    hash.addData(concat_ip_id.toUtf8());

    ipid = hash.result().toHex().right(8); // Use the last 8 characters (4 bytes)
    qDebug() << "IP:" << remote_ip.toString() << "HDID:" << p_hwid << "IPID:" << ipid;
}

void AOClient::sendServerMessage(QString message)
{
    sendPacket("CT", {"Server", message, "1"});
}

bool AOClient::checkAuth(unsigned long long acl_mask)
{
    if (acl_mask != ACLFlags.value("NONE")) {
        if (!authenticated) {
            return false;
        }

        QSettings settings("config/config.ini", QSettings::IniFormat);
        settings.beginGroup("Options");
        QString auth_type = settings.value("auth", "simple").toString();
        qDebug() << "auth type" << auth_type;
        if (auth_type == "advanced") {
            unsigned long long user_acl = server->db_manager->getACL(moderator_name);
            qDebug() << "checking with advanced auth";
            qDebug() << "got acl" << QString::number(user_acl, 16).toUpper() << "for user" << moderator_name;
            return (user_acl & acl_mask) != 0;
        }
        else if (auth_type == "simple") {
            return authenticated;
        }
    }
    return true;
}

QString AOClient::getIpid() { return ipid; }

AOClient::~AOClient() {
    socket->deleteLater();
}
