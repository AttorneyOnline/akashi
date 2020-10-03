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
    char_id = argv[1].toInt(&argument_ok);
    if (!argument_ok) {
        char_id = -1;
        return;
    }

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
    AOPacket validated_packet = validateIcPacket(packet);
    if (validated_packet.header == "INVALID")
        return;

    server->broadcast(validated_packet, current_area);
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

AOPacket AOClient::validateIcPacket(AOPacket packet)
{
    // Welcome to the super cursed server-side IC chat validation hell

    // I wanted to use enums or #defines here to make the
    // indicies of the args arrays more readable. But,
    // in typical AO fasion, the indicies for the incoming
    // and outgoing packets are different. Just RTFM.

    AOPacket invalid("INVALID", {});

    QStringList args;
    if (current_char == "" || !joined)
        // Spectators cannot use IC
        return invalid;

    QList<QVariant> incoming_args;
    for (QString arg : packet.contents) {
        incoming_args.append(QVariant(arg));
    }

    // message type
    if (incoming_args[0].toInt() == 1)
        args.append("1");
    else if (incoming_args[0].toInt() == 0) {
        if (incoming_args[0].toString() == "chat")
            args.append("chat");
        else
            args.append("0");
    }

    // preanim
    args.append(incoming_args[1].toString());

    // char name
    if (!server->characters.contains(incoming_args[2].toString()))
        return invalid;
    if (current_char != incoming_args[2].toString()) {
        // Selected char is different from supplied folder name
        // This means the user is INI-swapped
        // TODO: ini swap locking
        qDebug() << "INI swap detected from " << getIpid();
    }
    args.append(incoming_args[2].toString());

    // emote
    args.append(incoming_args[3].toString());

    // message text
    QString incoming_msg = incoming_args[4].toString().trimmed();
    if (incoming_msg == "") {
        if (last_msg_blankpost)
            return invalid;
        last_msg_blankpost = true;
    }
    else
        last_msg_blankpost = false;

    if (incoming_msg == last_message)
        return invalid;

    last_message = incoming_msg;
    args.append(incoming_msg);

    // side
    // this is validated clientside so w/e
    args.append(incoming_args[5].toString());

    // sfx name
    args.append(incoming_args[6].toString());

    // emote modifier
    // Now, gather round, y'all. Here is a story that is truly a microcosm of the AO dev experience.
    // If this value is a 4, it will crash the client. Why? Who knows, but it does.
    // Now here is the kicker: in certain versions, the client would incorrectly send a 4 here
    // For a long time, by configuring the client to do a zoom with a preanim, it would send 4
    // This would crash everyone else's client, and the feature had to be disabled
    // But, for some reason, nobody traced the cause of this issue for many many years.
    // The serverside fix is needed to ensure invalid values are not sent, because the client sucks
    int emote_mod = incoming_args[7].toInt();

    if (emote_mod == 4)
        emote_mod = 6;
    if (emote_mod != 0 && emote_mod != 1 && emote_mod != 2 && emote_mod != 5 && emote_mod != 6)
        return invalid;
    args.append(QString::number(emote_mod));

    // char id
    if (incoming_args[8].toInt() != char_id)
        return invalid;
    args.append(incoming_args[8].toString());

    // sfx delay
    args.append(incoming_args[9].toString());

    // objection modifier
    if (incoming_args[10].toString().contains("4")) {
        // custom shout includes text metadata
        args.append(incoming_args[10].toString());
    }
    else {
        int obj_mod = incoming_args[10].toInt();
        if (obj_mod != 0 && obj_mod != 1 && obj_mod != 2 && obj_mod != 3)
            return invalid;
        args.append(QString::number(obj_mod));
    }

    // evidence
    // TODO: add to this once evidence is implemented
    args.append(incoming_args[11].toString());

    // flipping
    int flip = incoming_args[12].toInt();
    if (flip != 0 && flip != 1)
        return invalid;
    args.append(QString::number(flip));

    // realization
    int realization = incoming_args[13].toInt();
    if (realization != 0 && realization != 1)
        return invalid;
    args.append(QString::number(realization));

    // text color
    int text_color = incoming_args[14].toInt();
    if (text_color != 0 && text_color != 1 && text_color != 2 && text_color != 3 && text_color != 4 && text_color != 5 && text_color != 6)
        return invalid;
    args.append(QString::number(text_color));

    // 2.6 packet extensions
    if (incoming_args.length() > 15) {

    }

    // 2.8 packet extensions
    if (incoming_args.length() > 19) {

    }

    return AOPacket("MS", args);
}
