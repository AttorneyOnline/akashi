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
}

void AOClient::clientData()
{
    QString data = QString::fromUtf8(socket->readAll());
    // qDebug() << "From" << client->peerAddress() << ":" << data;

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
    if (joined)
        server->player_count--;
    if (current_char != "")
        server->areas.value(current_area)->characters_taken[current_char] =
            false;
    server->updateCharsTaken(server->areas.value(current_area));
}

void AOClient::handlePacket(AOPacket packet)
{
    // TODO: like everything here should send a signal
    qDebug() << "Received packet:" << packet.header << ":" << packet.contents;
    AreaData* area = server->areas.value(current_area);
    // Lord forgive me
    if (packet.header == "HI") {
        setHwid(packet.contents[0]);

        AOPacket response(
            "ID", {"271828", "akashi", QApplication::applicationVersion()});
        sendPacket(response);
    }
    else if (packet.header == "ID") {
        QSettings config("config.ini", QSettings::IniFormat);
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
            "arup",         "casing_alserts",   "modcall_reason",
            "looping_sfx",  "additive",         "effects"};
        AOPacket response_pn(
            "PN", {QString::number(server->player_count), max_players});
        AOPacket response_fl("FL", feature_list);
        sendPacket(response_pn);
        sendPacket(response_fl);
    }
    else if (packet.header == "askchaa") {
        // TODO: add user configurable content
        // For testing purposes, we will just send enough to get things working
        AOPacket response(
            "SI", {QString::number(server->characters.length()), "0", "1"});
        sendPacket(response);
    }
    else if (packet.header == "RC") {
        AOPacket response("SC", server->characters);
        sendPacket(response);
    }
    else if (packet.header == "RM") {
        AOPacket response("SM", {"~stop.mp3"});
        sendPacket(response);
    }
    else if (packet.header == "RD") {
        server->player_count++;
        joined = true;

        server->updateCharsTaken(area);
        AOPacket response_op("OPPASS", {"DEADBEEF"});
        AOPacket response_done("DONE", {});
        sendPacket(response_op);
        sendPacket(response_done);
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

        server->updateCharsTaken(server->areas.value(current_area));
        AOPacket response_pv("PV", {"271828", "CID", packet.contents[1]});
        sendPacket(response_pv);
    }
    else if (packet.header == "MS") {
        // TODO: validate, validate, validate
        server->broadcast(packet);
    }
    else if (packet.header == "CT") {
        // TODO: commands
        // TODO: zalgo strip
        server->broadcast(packet);
    }
    else if (packet.header == "CH") {
        // Why does this packet exist
        AOPacket response("CHECK", {});
        sendPacket(response);
    }
    else if (packet.header == "whoami") {
        AOPacket response(
            "CT", {"Made with love", "by scatterflower and windrammer"});
        sendPacket(response);
    }
    else {
        qDebug() << "Unimplemented packet:" << packet.header;
        qDebug() << packet.contents;
    }
    socket->flush();
}

void AOClient::sendPacket(AOPacket packet)
{
    qDebug() << "Sent packet:" << packet.header << ":" << packet.contents;
    socket->write(packet.toUtf8());
    socket->flush();
}

QString AOClient::getHwid() { return hwid; }

void AOClient::setHwid(QString p_hwid)
{
    hwid = p_hwid;

    QCryptographicHash hash(
        QCryptographicHash::Md5); // Don't need security, just
                                  // hashing for uniqueness
    QString concat_ip_id = remote_ip.toString() + p_hwid;
    hash.addData(concat_ip_id.toUtf8());

    ipid = hash.result().toHex().right(8);
}

QString AOClient::getIpid() { return ipid; }

AOClient::~AOClient() {}
