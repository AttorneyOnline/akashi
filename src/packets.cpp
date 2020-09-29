//////////////////////////////////////////////////////////////////////////////////////
//    akashi - a server for Attorney Online 2                                       //
//    Copyright (C) 2020  scatterflower                                           //
//                                                                                  //
//    This program is free software: you can redistribute it and/or modify          //
//    it under the terms of the GNU Affero General Public License as                //
//    published by the Free Software Foundation, either version 3 of the            //
//    License, or (at your option) any later version.                               //
//                                                                                  //
//    This program is distributed in the hope that it will be useful,               //
//    but WITHOUT ANY WARRANTY{} without even the implied warranty of                //
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                 //
//    GNU Affero General Public License for more details.                           //
//                                                                                  //
//    You should have received a copy of the GNU Affero General Public License      //
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.        //
//////////////////////////////////////////////////////////////////////////////////////
#include "include/aoclient.h"

void AOClient::pktDefault(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    qDebug() << "Unimplemented packet:" << packet.header;
    qDebug() << packet.contents;
}

void AOClient::pktHardwareId(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    setHwid(argv[0]);
    if(server->db_manager->isHDIDBanned(getHwid())) {
        sendPacket("BD", {server->db_manager->getBanReason(getHwid())});
        socket->close();
        return;
    }
    sendPacket("ID", {"271828", "akashi", QCoreApplication::applicationVersion()});
}

void AOClient::pktSoftwareId(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
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

void AOClient::pktBeginLoad(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    // TODO: add user configurable content
    // For testing purposes, we will just send enough to get things working
    sendPacket("SI", {QString::number(server->characters.length()), "0", QString::number(server->area_names.length() + server->music_list.length())});
}

void AOClient::pktRequestChars(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    sendPacket("SC", server->characters);
}

void AOClient::pktRequestMusic(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    sendPacket("SM", server->area_names + server->music_list);
}

void AOClient::pktLoadingDone(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    if (getHwid() == "") {
        // No early connecting!
        socket->close();
        return;
    }

    if (joined) {
        return;
    }

    server->player_count++;
    area->player_count++;
    joined = true;
    server->updateCharsTaken(area);
    fullArup(); // Give client all the area data
    arup(ARUPType::PLAYER_COUNT, true); // Tell everyone there is a new player

    sendPacket("HP", {"1", QString::number(area->def_hp)});
    sendPacket("HP", {"2", QString::number(area->pro_hp)});
    sendPacket("FA", server->area_names);
    sendPacket("BN", {area->background});
    sendPacket("OPPASS", {"DEADBEEF"});
    sendPacket("DONE");
}

void AOClient::pktCharPassword(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    password = argv[0];
}

void AOClient::pktSelectChar(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    bool argument_ok;
    int char_id = argv[1].toInt(&argument_ok);
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
    sendPacket("PV", {"271828", "CID", argv[1]});
}

void AOClient::pktIcChat(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    // TODO: validate, validate, validate
    ICChatPacket ic_packet(packet);
    if (ic_packet.is_valid)
        server->broadcast(ic_packet, current_area);
}

void AOClient::pktOocChat(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    ooc_name = argv[0];
    if(argv[1].at(0) == '/') {
        QStringList cmd_argv = argv[1].split(" ", QString::SplitBehavior::SkipEmptyParts);
        QString command = cmd_argv[0].trimmed().toLower();
        command = command.right(command.length() - 1);
        cmd_argv.removeFirst();
        int cmd_argc = cmd_argv.length();
        handleCommand(command, cmd_argc, cmd_argv);
        return;
    }
    // TODO: zalgo strip
    server->broadcast(packet, current_area);
}

void AOClient::pktPing(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    // Why does this packet exist
    // At least Crystal made it useful
    // It is now used for ping measurement
    sendPacket("CHECK");
}

void AOClient::pktChangeMusic(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    // Due to historical reasons, this
    // packet has two functions:
    // Change area, and set music.

    // First, we check if the provided
    // argument is a valid song
    QString argument = argv[0];

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

void AOClient::pktWtCe(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    if (QDateTime::currentDateTime().toSecsSinceEpoch() - last_wtce_time <= 5)
        return;
    last_wtce_time = QDateTime::currentDateTime().toSecsSinceEpoch();
    server->broadcast(packet, current_area);
}

void AOClient::pktHpBar(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    if (argv[0] == "1") {
        area->def_hp = std::min(std::max(0, argv[1].toInt()), 10);
    }
    else if (argv[0] == "2") {
        area->pro_hp = std::min(std::max(0, argv[1].toInt()), 10);
    }
    server->broadcast(AOPacket("HP", {"1", QString::number(area->def_hp)}), area->index);
    server->broadcast(AOPacket("HP", {"2", QString::number(area->pro_hp)}), area->index);
}

void AOClient::pktWebSocketIp(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    // Special packet to set remote IP from the webao proxy
    // Only valid if from a local ip
    if (remote_ip.isLoopback()) {
        if(server->db_manager->isIPBanned(QHostAddress(argv[0]))) {
            sendPacket("BD", {server->db_manager->getBanReason(QHostAddress(argv[0]))});
            socket->close();
            return;
        }
        qDebug() << "ws ip set to" << argv[0];
        remote_ip = QHostAddress(argv[0]);
    }
}
