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
    if (joined)
        server->player_count--;
    if (current_char != "") {
        server->areas.value(current_area)->characters_taken[current_char] =
            false;
        server->updateCharsTaken(server->areas.value(current_area));
    }
}

void AOClient::handlePacket(AOPacket packet)
{
    // TODO: like everything here should send a signal
    //qDebug() << "Received packet:" << packet.header << ":" << packet.contents;
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
            "arup",         "casing_alerts",    "modcall_reason",
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
            "SI", {QString::number(server->characters.length()), "0", QString::number(server->area_names.length() + server->music_list.length())});
        sendPacket(response);
    }
    else if (packet.header == "RC") {
        AOPacket response("SC", server->characters);
        sendPacket(response);
    }
    else if (packet.header == "RM") {
        AOPacket response("SM", server->area_names + server->music_list);
        sendPacket(response);
    }
    else if (packet.header == "RD") {
        server->player_count++;
        joined = true;
        server->updateCharsTaken(area);

        QSettings areas_ini("areas.ini", QSettings::IniFormat);
        QStringList areas = areas_ini.childGroups();

        AOPacket response_fa("FA", areas);
        AOPacket response_op("OPPASS", {"DEADBEEF"});
        AOPacket response_done("DONE", {});
        sendPacket(response_fa);
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

        server->updateCharsTaken(server->areas.value(current_area));
        AOPacket response_pv("PV", {"271828", "CID", packet.contents[1]});
        sendPacket(response_pv);
    }
    else if (packet.header == "MS") {
        // TODO: validate, validate, validate
        server->broadcast(packet, current_area);
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
        AOPacket response("CHECK", {});
        sendPacket(response);
    }
    else if (packet.header == "whoami") {
        AOPacket response(
            "CT", {"Made with love", "by scatterflower and windrammer"});
        sendPacket(response);
    }
    else if (packet.header == "MC") {
        // Due to historical reasons, this
        // packet has two functions:
        // Change area, and set music.

        // First, we check if the provided
        // argument is a valid song
        QString argument = packet.contents[0];

        bool is_song = false;
        for (QString song : server->music_list) {
            if (song == argument) {
                is_song = true;
                break;
            }
        }

        if (is_song) {
            // If we have a song, retransmit as-is
            server->broadcast(packet, current_area);
            return;
        }

        bool is_area = false;
        for (QString area : server->area_names) {
            if(area == argument) {
                is_area = true;
                break;
            }
        }

        if (is_area) {
            // TODO: change area function that resends all area data and sets user stuff
            // For now, we pretend
            AOPacket user_message("CT", {"Server", "Changed to area " + argument});
            sendPacket(user_message);
        }
    }
    else {
        qDebug() << "Unimplemented packet:" << packet.header;
        qDebug() << packet.contents;
    }
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

AOClient::~AOClient() {}
