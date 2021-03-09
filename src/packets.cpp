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
#ifdef NET_DEBUG
    qDebug() << "Unimplemented packet:" << packet.header << packet.contents;
#endif
}

void AOClient::pktHardwareId(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    setHwid(argv[0]);
    if(server->db_manager->isHDIDBanned(getHwid())) {
        sendPacket("BD", {server->db_manager->getBanReason(getHwid())});
        socket->close();
        return;
    }
    sendPacket("ID", {QString::number(id), "akashi", QCoreApplication::applicationVersion()});
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
    QStringList feature_list = {
        "noencryption", "yellowtext",       "prezoom",
        "flipping",     "customobjections", "fastloading",
        "deskmod",      "evidence",         "cccc_ic_support",
        "arup",         "casing_alerts",    "modcall_reason",
        "looping_sfx",  "additive",         "effects",
        "y_offset"
    };

    sendPacket("PN", {QString::number(server->player_count), max_players});
    sendPacket("FL", feature_list);
}

void AOClient::pktBeginLoad(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    // Evidence isn't loaded during this part anymore
    // As a result, we can always send "0" for evidence length
    // Client only cares about what it gets from LE
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
    sendEvidenceList(area);

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

    pos = "";

    server->updateCharsTaken(area);
    sendPacket("PV", {QString::number(id), "CID", argv[1]});
    fullArup();
    if (server->timer->isActive()) {
        sendPacket("TI", {QString::number(0), QString::number(2)});
        sendPacket("TI", {QString::number(0), QString::number(0), QString::number(QTime(0,0).msecsTo(QTime(0,0).addMSecs(server->timer->remainingTime())))});
    }
    else {
        sendPacket("TI", {QString::number(0), QString::number(3)});
    }
    for (QTimer* timer : area->timers) {
        int timer_id = area->timers.indexOf(timer) + 1;
        if (timer->isActive()) {
            sendPacket("TI", {QString::number(timer_id), QString::number(2)});
            sendPacket("TI", {QString::number(timer_id), QString::number(0), QString::number(QTime(0,0).msecsTo(QTime(0,0).addMSecs(timer->remainingTime())))});
        }
        else {
            sendPacket("TI", {QString::number(timer_id), QString::number(3)});
        }
    }
}

void AOClient::pktIcChat(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    AOPacket validated_packet = validateIcPacket(packet);
    if (validated_packet.header == "INVALID")
        return;

    if (pos != "")
        validated_packet.contents[5] = pos;

    area->logger->logIC(this, &validated_packet);
    server->broadcast(validated_packet, current_area);
}

void AOClient::pktOocChat(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    ooc_name = dezalgo(argv[0]);
    QString message = dezalgo(argv[1]);
    AOPacket final_packet("CT", {ooc_name, message, "0"});
    if(message.at(0) == '/') {
        QStringList cmd_argv = message.split(" ", QString::SplitBehavior::SkipEmptyParts);
        QString command = cmd_argv[0].trimmed().toLower();
        command = command.right(command.length() - 1);
        cmd_argv.removeFirst();
        int cmd_argc = cmd_argv.length();
        area->logger->logCmd(this, &final_packet, command, cmd_argv);
        handleCommand(command, cmd_argc, cmd_argv);
    }
    else {
        server->broadcast(final_packet, current_area);
        area->logger->logOOC(this, &final_packet);
    }
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
            // We have a song here
            AOPacket music_change("MC", {song, argv[1], argv[2], "1", "0", argv[3]});
            server->broadcast(music_change, current_area);
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
#ifdef NET_DEBUG
        qDebug() << "ws ip set to" << argv[0];
#endif
        remote_ip = QHostAddress(argv[0]);
    }
}

void AOClient::pktModCall(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    for (AOClient* client : server->clients) {
        if (client->authenticated)
            client->sendPacket(packet);
    }
    area->logger->logModcall(this, &packet);
    area->logger->flush();
}

void AOClient::pktAddEvidence(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    AreaData::Evidence evi = {argv[0], argv[1], argv[2]};
    area->evidence.append(evi);
    sendEvidenceList(area);
}

void AOClient::pktRemoveEvidence(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    bool is_int = false;
    int idx = argv[0].toInt(&is_int);
    if (is_int && idx <= area->evidence.size() && idx >= 0) {
        area->evidence.removeAt(idx);
    }
    sendEvidenceList(area);
}

void AOClient::pktEditEvidence(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    bool is_int = false;
    int idx = argv[0].toInt(&is_int);
    AreaData::Evidence evi = {argv[1], argv[2], argv[3]};
    if (is_int && idx <= area->evidence.size() && idx >= 0) {
        area->evidence.replace(idx, evi);
    }
    sendEvidenceList(area);
}

void AOClient::sendEvidenceList(AreaData* area)
{
    QStringList evidence_list;
    QString evidence_format("%1&%2&%3");

    for (AreaData::Evidence evidence : area->evidence) {
        evidence_list.append(evidence_format
                             .arg(evidence.name)
                             .arg(evidence.description)
                             .arg(evidence.image));
    }

    server->broadcast(AOPacket("LE", evidence_list), current_area);
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
    AreaData* area = server->areas[current_area];
    if (area->locked == AreaData::LockStatus::SPECTATABLE && !area->invited.contains(id))
        // Non-invited players cannot speak in spectatable areas
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
    if (current_char != incoming_args[2].toString()) {
        // Selected char is different from supplied folder name
        // This means the user is INI-swapped
        // TODO: ini swap locking
        // if no iniswap allowed then
        // if (!server->characters.contains(incoming_args[2].toString()))
        //     return invalid;
        qDebug() << "INI swap detected from " << getIpid();
    }
    args.append(incoming_args[2].toString());

    // emote
    emote = incoming_args[3].toString();
    args.append(emote);

    // message text
    QString incoming_msg = dezalgo(incoming_args[4].toString().trimmed());
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
    int evi_idx = incoming_args[11].toInt();
    if (evi_idx > area->evidence.length())
        return invalid;
    args.append(QString::number(evi_idx));

    // flipping
    int flip = incoming_args[12].toInt();
    if (flip != 0 && flip != 1)
        return invalid;
    flipping = QString::number(flip);
    args.append(flipping);

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
        // showname
        QString incoming_showname = dezalgo(incoming_args[15].toString().trimmed());
        args.append(incoming_showname);
        showname = incoming_showname;

        // other char id
        // things get a bit hairy here
        // don't ask me how this works, because i don't know either
        QStringList pair_data = incoming_args[16].toString().split("^");
        pairing_with = pair_data[0].toInt();
        QString front_back = "";
        if (pair_data.length() > 1)
            front_back = "^" + pair_data[1];
        int other_charid = pairing_with;
        bool pairing = false;
        QString other_name = "0";
        QString other_emote = "0";
        QString other_offset = "0";
        QString other_flip = "0";
        for (AOClient* client : server->clients) {
            if (client->pairing_with == char_id && other_charid != char_id && client->char_id == pairing_with) {
                other_name = server->characters.at(other_charid);
                other_emote = client->emote;
                other_offset = client->offset;
                other_flip = client->flipping;
                pairing = true;
            }
        }
        if (!pairing) {
            other_charid = -1;
            front_back = "";
        }
        args.append(QString::number(other_charid) + front_back);
        args.append(other_name);
        args.append(other_emote);

        // self offset
        offset = incoming_args[17].toString();
        args.append(offset);
        args.append(other_offset);
        args.append(other_flip);

        // noninterrupting preanim
        int ni_pa = incoming_args[18].toInt();
        if (ni_pa != 1 && ni_pa != 0)
            return invalid;
        args.append(QString::number(ni_pa));
    }

    // 2.8 packet extensions
    if (incoming_args.length() > 19) {
        // sfx looping
        int sfx_loop = incoming_args[19].toInt();
        if (sfx_loop != 0 && sfx_loop != 1)
            return invalid;
        args.append(QString::number(sfx_loop));

        // screenshake
        int screenshake = incoming_args[20].toInt();
        if (screenshake != 0 && screenshake != 1)
            return invalid;
        args.append(QString::number(screenshake));

        // frames shake
        args.append(incoming_args[21].toString());

        // frames realization
        args.append(incoming_args[22].toString());

        // frames sfx
        args.append(incoming_args[23].toString());

        // additive
        int additive = incoming_args[24].toInt();
        if (additive != 0 && additive != 1)
            return invalid;
        args.append(QString::number(additive));

        // effect
        args.append(incoming_args[25].toString());
    }

    return AOPacket("MS", args);
}

QString AOClient::dezalgo(QString p_text)
{
    QSettings config("config/config.ini", QSettings::IniFormat);
    config.beginGroup("Options");
    bool zalgo_tolerance_conversion_success;
    int zalgo_tolerance = config.value("zalgo_tolerance", "3").toInt(&zalgo_tolerance_conversion_success);
    if (!zalgo_tolerance_conversion_success)
        zalgo_tolerance = 3;

    QRegExp rxp("([\u0300-\u036f\u1ab0-\u1aff\u1dc0-\u1dff\u20d0-\u20ff\ufe20-\ufe2f\u115f\u1160\u3164]{" + QRegExp::escape(QString::number(zalgo_tolerance)) + ",})");
    QString filtered = p_text.replace(rxp, "");
    return filtered;
}
