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
    qDebug() << "From" << remote_ip << ":" << data;

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
    qDebug() << remote_ip << "disconnected";
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
    // TODO: like everything here should send a signal
    //qDebug() << "Received packet:" << packet.header << ":" << packet.contents;
    AreaData* area = server->areas[current_area];
    // Lord forgive me
    if (packet.header == "HI") {
        setHwid(packet.contents[0]);
        sendPacket("ID", {"271828", "akashi", QCoreApplication::applicationVersion()});
    }
    else if (packet.header == "ID") {
        QSettings config("config/config.ini", QSettings::IniFormat);
        config.beginGroup("Options");
        QString max_players = config.value("max_players").toString();
        config.endGroup();

        // Full feature list as of AO 2.8.5
        // The only ones that are critical to ensuring the server works are
        // "noencryption" and "fastloading"
        // TODO: make the rest of these user configurable
        QStringList feature_list = {
            "noencryption", "yellowtext",       "prezoom",
            "flipping",     "customobjections", "fastloading",
            "deskmod",      "evidence",         "cccc_ic_support",
            "arup",         "casing_alerts",    "modcall_reason",
            "looping_sfx",  "additive",         "effects"};

        sendPacket("PN", {QString::number(server->player_count), max_players});
        sendPacket("FL", feature_list);
    }
    else if (packet.header == "askchaa") {
        // TODO: add user configurable content
        // For testing purposes, we will just send enough to get things working
        sendPacket("SI", {QString::number(server->characters.length()), "0", QString::number(server->area_names.length() + server->music_list.length())});
    }
    else if (packet.header == "RC") {
        sendPacket("SC", server->characters);
    }
    else if (packet.header == "RM") {
        sendPacket("SM", server->area_names + server->music_list);
    }
    else if (packet.header == "RD") {
        server->player_count++;
        area->player_count++;
        joined = true;
        server->updateCharsTaken(area);
        fullArup(); // Give client all the area data
        arup(ARUPType::PLAYER_COUNT, true); // Tell everyone there is a new player

        sendPacket("HP", {"1", QString::number(area->def_hp)});
        sendPacket("HP", {"2", QString::number(area->pro_hp)});
        sendPacket("FA", server->area_names);
        sendPacket("OPPASS", {"DEADBEEF"});
        sendPacket("DONE");
    }
    else if (packet.header == "PW") {
        password = packet.contents[0];
    }
    else if (packet.header == "CC") {
        bool argument_ok;
        int char_id = packet.contents[1].toInt(&argument_ok);
        if (!argument_ok)
            return;

        if (current_char != "") {
            area->characters_taken[current_char] = false;
        }

        if(char_id > server->characters.length())
            return;

        if (char_id >= 0) {
            QString char_selected = server->characters[char_id];
            bool taken = area->characters_taken.value(char_selected);
            if (taken || char_selected == "")
                return;

            area->characters_taken[char_selected] = true;
            current_char = char_selected;
        }
        else {
            current_char = "";
        }

        server->updateCharsTaken(area);
        sendPacket("PV", {"271828", "CID", packet.contents[1]});
    }
    else if (packet.header == "MS") {
        // TODO: validate, validate, validate
        ICChatPacket ic_packet(packet);
        if (ic_packet.is_valid)
            server->broadcast(ic_packet, current_area);
    }
    else if (packet.header == "CT") {
        // TODO: commands
        // TODO: zalgo strip
        server->broadcast(packet, current_area);
    }
    else if (packet.header == "CH") {
        // Why does this packet exist
        // At least Crystal made it useful
        // It is now used for ping measurement
        sendPacket("CHECK");
    }
    else if (packet.header == "MC") {
        // Due to historical reasons, this
        // packet has two functions:
        // Change area, and set music.

        // First, we check if the provided
        // argument is a valid song
        QString argument = packet.contents[0];

        for (QString song : server->music_list) {
            if (song == argument) {
                // If we have a song, retransmit as-is
                server->broadcast(packet, current_area);
                return;
            }
        }

        for (int i = 0; i < server->area_names.length(); i++) {
            QString area = server->area_names[i];
            if(area == argument) {
                changeArea(i);
                break;
            }
        }
    }
    else if (packet.header == "RT") {
        if (QDateTime::currentDateTime().toSecsSinceEpoch() - last_wtce_time <= 5)
            return;
        last_wtce_time = QDateTime::currentDateTime().toSecsSinceEpoch();
        server->broadcast(packet, current_area);
    }
    else if (packet.header == "HP") {
        if (packet.contents[0] == "1") {
            area->def_hp = std::min(std::max(0, packet.contents[1].toInt()), 10);
        }
        else if (packet.contents[0] == "2") {
            area->pro_hp = std::min(std::max(0, packet.contents[1].toInt()), 10);
        }
        server->broadcast(AOPacket("HP", {"1", QString::number(area->def_hp)}), area->index);
        server->broadcast(AOPacket("HP", {"2", QString::number(area->pro_hp)}), area->index);
    }
    else {
        qDebug() << "Unimplemented packet:" << packet.header;
        qDebug() << packet.contents;
    }
}

void AOClient::changeArea(int new_area)
{
    // TODO: function to send chat messages with hostname automatically
    if (current_area == new_area) {
        sendPacket("CT", {"Server", "You are already in area " + server->area_names[current_area], "1"});
        return;
    }
    if (server->areas[new_area]->locked) {
        sendPacket("CT", {"Server", "Area " + server->area_names[new_area] + " is locked.", "1"});
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
    // send hp, bn, le
    sendPacket("HP", {"1", QString::number(server->areas[new_area]->def_hp)});
    sendPacket("HP", {"2", QString::number(server->areas[new_area]->pro_hp)});
    if (server->areas[current_area]->characters_taken[current_char]) {
        server->updateCharsTaken(server->areas[current_area]);
        current_char = "";
        sendPacket("DONE");
    }
    else {
        server->areas[current_area]->characters_taken[current_char] = true;
        server->updateCharsTaken(server->areas[current_area]);
    }
    sendPacket("CT", {"Server", "You have been moved to area " + server->area_names[current_area], "1"});
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

    ipid = hash.result().toHex().right(8);
}

QString AOClient::getIpid() { return ipid; }

AOClient::~AOClient() {
    socket->deleteLater();
}
