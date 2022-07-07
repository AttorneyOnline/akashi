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

#include "include/area_data.h"
#include "include/packet/packet_factory.h"
#include "include/server.h"

// This file is for commands under the messaging category in aoclient.h
// Be sure to register the command in the header before adding it here!

void AOClient::cmdPos(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    changePosition(argv[0]);
    updateEvidenceList(server->getAreaById(m_current_area));
}

void AOClient::cmdForcePos(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    bool ok;
    QList<AOClient *> l_targets;
    int l_target_id = argv[1].toInt(&ok);
    int l_forced_clients = 0;
    if (!ok && argv[1] != "*") {
        sendServerMessage("That does not look like a valid ID.");
        return;
    }
    else if (ok) {
        AOClient *l_target_client = server->getClientByID(l_target_id);
        if (l_target_client != nullptr)
            l_targets.append(l_target_client);
        else {
            sendServerMessage("Target ID not found!");
            return;
        }
    }

    else if (argv[1] == "*") { // force all clients in the area
        const QVector<AOClient *> l_clients = server->getClients();
        for (AOClient *l_client : l_clients) {
            if (l_client->m_current_area == m_current_area)
                l_targets.append(l_client);
        }
    }
    for (AOClient *l_target : l_targets) {
        l_target->sendServerMessage("Position forcibly changed by CM.");
        l_target->changePosition(argv[0]);
        l_forced_clients++;
    }
    sendServerMessage("Forced " + QString::number(l_forced_clients) + " into pos " + argv[0] + ".");
}

void AOClient::cmdG(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    QString l_sender_name = m_ooc_name;
    QString l_sender_area = server->getAreaName(m_current_area);
    QString l_sender_message = argv.join(" ");
    // Better readability thanks to AwesomeAim.
    AOPacket *l_mod_packet = PacketFactory::createPacket("CT", {"[G][" + m_ipid + "][" + l_sender_area + "]" + l_sender_name, l_sender_message});
    AOPacket *l_user_packet = PacketFactory::createPacket("CT", {"[G][" + l_sender_area + "]" + l_sender_name, l_sender_message});
    server->broadcast(l_user_packet, l_mod_packet, Server::TARGET_TYPE::AUTHENTICATED);
    return;
}

void AOClient::cmdNeed(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    QString l_sender_area = server->getAreaName(m_current_area);
    QString l_sender_message = argv.join(" ");
    server->broadcast(PacketFactory::createPacket("CT", {"=== Advert ===\n[" + l_sender_area + "] needs " + l_sender_message + "."}), Server::TARGET_TYPE::ADVERT);
}

void AOClient::cmdSwitch(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    int l_selected_char_id = server->getCharID(argv.join(" "));
    if (l_selected_char_id == -1) {
        sendServerMessage("That does not look like a valid character.");
        return;
    }
    if (changeCharacter(l_selected_char_id)) {
        m_char_id = l_selected_char_id;
    }
    else {
        sendServerMessage("The character you picked is either taken or invalid.");
    }
}

void AOClient::cmdRandomChar(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    AreaData *l_area = server->getAreaById(m_current_area);
    int l_selected_char_id;
    bool l_taken = true;
    while (l_taken) {
        l_selected_char_id = genRand(0, server->getCharacterCount() - 1);
        if (!l_area->charactersTaken().contains(l_selected_char_id)) {
            l_taken = false;
        }
    }
    if (changeCharacter(l_selected_char_id)) {
        m_char_id = l_selected_char_id;
    }
}

void AOClient::cmdToggleGlobal(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    m_global_enabled = !m_global_enabled;
    QString l_str_en = m_global_enabled ? "shown" : "hidden";
    sendServerMessage("Global chat set to " + l_str_en);
}

void AOClient::cmdPM(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    bool ok;
    int l_target_id = argv.takeFirst().toInt(&ok); // using takeFirst removes the ID from our list of arguments...
    if (!ok) {
        sendServerMessage("That does not look like a valid ID.");
        return;
    }
    AOClient *l_target_client = server->getClientByID(l_target_id);
    if (l_target_client == nullptr) {
        sendServerMessage("No client with that ID found.");
        return;
    }
    if (l_target_client->m_pm_mute) {
        sendServerMessage("That user is not recieving PMs.");
        return;
    }
    QString l_message = argv.join(" "); //...which means it will not end up as part of the message
    l_target_client->sendServerMessage("Message from " + m_ooc_name + " (" + QString::number(m_id) + "): " + l_message);
    sendServerMessage("PM sent to " + QString::number(l_target_id) + ". Message: " + l_message);
}

void AOClient::cmdAnnounce(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    sendServerBroadcast("=== Announcement ===\r\n" + argv.join(" ") + "\r\n=============");
}

void AOClient::cmdM(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    QString l_sender_name = m_ooc_name;
    QString l_sender_message = argv.join(" ");
    server->broadcast(PacketFactory::createPacket("CT", {"[M]" + l_sender_name, l_sender_message}), Server::TARGET_TYPE::MODCHAT);
}

void AOClient::cmdGM(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    QString l_sender_name = m_ooc_name;
    QString l_sender_area = server->getAreaName(m_current_area);
    QString l_sender_message = argv.join(" ");
    server->broadcast(PacketFactory::createPacket("CT", {"[G][" + l_sender_area + "]" + "[" + l_sender_name + "][M]", l_sender_message}), Server::TARGET_TYPE::MODCHAT);
}

void AOClient::cmdLM(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    QString l_sender_name = m_ooc_name;
    QString l_sender_message = argv.join(" ");
    server->broadcast(PacketFactory::createPacket("CT", {"[" + l_sender_name + "][M]", l_sender_message}), m_current_area);
}

void AOClient::cmdGimp(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    bool conv_ok = false;
    int l_uid = argv[0].toInt(&conv_ok);
    if (!conv_ok) {
        sendServerMessage("Invalid user ID.");
        return;
    }

    AOClient *l_target = server->getClientByID(l_uid);

    if (l_target == nullptr) {
        sendServerMessage("No client with that ID found.");
        return;
    }

    if (l_target->m_is_gimped)
        sendServerMessage("That player is already gimped!");
    else {
        sendServerMessage("Gimped player.");
        l_target->sendServerMessage("You have been gimped! " + getReprimand());
    }
    l_target->m_is_gimped = true;
}

void AOClient::cmdUnGimp(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    bool conv_ok = false;
    int l_uid = argv[0].toInt(&conv_ok);
    if (!conv_ok) {
        sendServerMessage("Invalid user ID.");
        return;
    }

    AOClient *l_target = server->getClientByID(l_uid);

    if (l_target == nullptr) {
        sendServerMessage("No client with that ID found.");
        return;
    }

    if (!(l_target->m_is_gimped))
        sendServerMessage("That player is not gimped!");
    else {
        sendServerMessage("Ungimped player.");
        l_target->sendServerMessage("A moderator has ungimped you! " + getReprimand(true));
    }
    l_target->m_is_gimped = false;
}

void AOClient::cmdDisemvowel(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    bool conv_ok = false;
    int l_uid = argv[0].toInt(&conv_ok);
    if (!conv_ok) {
        sendServerMessage("Invalid user ID.");
        return;
    }

    AOClient *l_target = server->getClientByID(l_uid);

    if (l_target == nullptr) {
        sendServerMessage("No client with that ID found.");
        return;
    }

    if (l_target->m_is_disemvoweled)
        sendServerMessage("That player is already disemvoweled!");
    else {
        sendServerMessage("Disemvoweled player.");
        l_target->sendServerMessage("You have been disemvoweled! " + getReprimand());
    }
    l_target->m_is_disemvoweled = true;
}

void AOClient::cmdUnDisemvowel(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    bool conv_ok = false;
    int l_uid = argv[0].toInt(&conv_ok);
    if (!conv_ok) {
        sendServerMessage("Invalid user ID.");
        return;
    }

    AOClient *l_target = server->getClientByID(l_uid);

    if (l_target == nullptr) {
        sendServerMessage("No client with that ID found.");
        return;
    }

    if (!(l_target->m_is_disemvoweled))
        sendServerMessage("That player is not disemvoweled!");
    else {
        sendServerMessage("Undisemvoweled player.");
        l_target->sendServerMessage("A moderator has undisemvoweled you! " + getReprimand(true));
    }
    l_target->m_is_disemvoweled = false;
}

void AOClient::cmdShake(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    bool conv_ok = false;
    int l_uid = argv[0].toInt(&conv_ok);
    if (!conv_ok) {
        sendServerMessage("Invalid user ID.");
        return;
    }

    AOClient *l_target = server->getClientByID(l_uid);

    if (l_target == nullptr) {
        sendServerMessage("No client with that ID found.");
        return;
    }

    if (l_target->m_is_shaken)
        sendServerMessage("That player is already shaken!");
    else {
        sendServerMessage("Shook player.");
        l_target->sendServerMessage("A moderator has shaken your words! " + getReprimand());
    }
    l_target->m_is_shaken = true;
}

void AOClient::cmdUnShake(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    bool conv_ok = false;
    int l_uid = argv[0].toInt(&conv_ok);
    if (!conv_ok) {
        sendServerMessage("Invalid user ID.");
        return;
    }

    AOClient *l_target = server->getClientByID(l_uid);

    if (l_target == nullptr) {
        sendServerMessage("No client with that ID found.");
        return;
    }

    if (!(l_target->m_is_shaken))
        sendServerMessage("That player is not shaken!");
    else {
        sendServerMessage("Unshook player.");
        l_target->sendServerMessage("A moderator has unshook you! " + getReprimand(true));
    }
    l_target->m_is_shaken = false;
}

void AOClient::cmdMutePM(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    m_pm_mute = !m_pm_mute;
    QString l_str_en = m_pm_mute ? "muted" : "unmuted";
    sendServerMessage("PM's are now " + l_str_en);
}

void AOClient::cmdToggleAdverts(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    m_advert_enabled = !m_advert_enabled;
    QString l_str_en = m_advert_enabled ? "on" : "off";
    sendServerMessage("Advertisements turned " + l_str_en);
}

void AOClient::cmdAfk(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    m_is_afk = true;
    sendServerMessage("You are now AFK.");
}

void AOClient::cmdCharCurse(int argc, QStringList argv)
{
    bool conv_ok = false;
    int l_uid = argv[0].toInt(&conv_ok);
    if (!conv_ok) {
        sendServerMessage("Invalid user ID.");
        return;
    }

    AOClient *l_target = server->getClientByID(l_uid);

    if (l_target == nullptr) {
        sendServerMessage("No client with that ID found.");
        return;
    }

    if (l_target->m_is_charcursed) {
        sendServerMessage("That player is already charcursed!");
        return;
    }

    if (argc == 1) {
        l_target->m_charcurse_list.append(server->getCharID(l_target->m_current_char));
    }
    else {
        argv.removeFirst();
        QStringList l_char_names = argv.join(" ").split(",");

        l_target->m_charcurse_list.clear();
        for (const QString &l_char_name : qAsConst(l_char_names)) {
            int char_id = server->getCharID(l_char_name);
            if (char_id == -1) {
                sendServerMessage("Could not find character: " + l_char_name);
                return;
            }
            l_target->m_charcurse_list.append(char_id);
        }
    }

    l_target->m_is_charcursed = true;

    // Kick back to char select screen
    if (!l_target->m_charcurse_list.contains(server->getCharID(l_target->m_current_char))) {
        l_target->changeCharacter(-1);
        server->updateCharsTaken(server->getAreaById(m_current_area));
        l_target->sendPacket("DONE");
    }
    else {
        server->updateCharsTaken(server->getAreaById(m_current_area));
    }

    l_target->sendServerMessage("You have been charcursed!");
    sendServerMessage("Charcursed player.");
}

void AOClient::cmdUnCharCurse(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    bool conv_ok = false;
    int l_uid = argv[0].toInt(&conv_ok);
    if (!conv_ok) {
        sendServerMessage("Invalid user ID.");
        return;
    }

    AOClient *l_target = server->getClientByID(l_uid);

    if (l_target == nullptr) {
        sendServerMessage("No client with that ID found.");
        return;
    }

    if (!l_target->m_is_charcursed) {
        sendServerMessage("That player is not charcursed!");
        return;
    }
    l_target->m_is_charcursed = false;
    l_target->m_charcurse_list.clear();
    server->updateCharsTaken(server->getAreaById(m_current_area));
    sendServerMessage("Uncharcursed player.");
    l_target->sendServerMessage("You were uncharcursed.");
}

void AOClient::cmdCharSelect(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    changeCharacter(-1);
    sendPacket("DONE");
}

void AOClient::cmdForceCharSelect(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    bool ok = false;
    int l_target_id = argv[0].toInt(&ok);
    if (!ok) {
        sendServerMessage("This ID does not look valid. Please use the client ID.");
        return;
    }

    AOClient *l_target = server->getClientByID(l_target_id);

    if (l_target == nullptr) {
        sendServerMessage("Unable to locate client with ID " + QString::number(l_target_id) + ".");
        return;
    }

    l_target->changeCharacter(-1);
    l_target->sendPacket("DONE");
    sendServerMessage("Client has been forced into character select!");
}

void AOClient::cmdA(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    bool ok;
    int l_area_id = argv[0].toInt(&ok);
    if (!ok) {
        sendServerMessage("This does not look like a valid AreaID.");
        return;
    }

    AreaData *l_area = server->getAreaById(l_area_id);
    if (!l_area->owners().contains(m_id)) {
        sendServerMessage("You are not CM in that area.");
        return;
    }

    argv.removeAt(0);
    QString l_sender_name = m_ooc_name;
    QString l_ooc_message = argv.join(" ");
    server->broadcast(PacketFactory::createPacket("CT", {"[CM]" + l_sender_name, l_ooc_message}), l_area_id);
}

void AOClient::cmdS(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    int l_all_areas = server->getAreaCount() - 1;
    QString l_sender_name = m_ooc_name;
    QString l_ooc_message = argv.join(" ");

    for (int i = 0; i <= l_all_areas; i++) {
        if (server->getAreaById(i)->owners().contains(m_id))
            server->broadcast(PacketFactory::createPacket("CT", {"[CM]" + l_sender_name, l_ooc_message}), i);
    }
}

void AOClient::cmdFirstPerson(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    m_first_person = !m_first_person;
    QString l_str_en = m_first_person ? "enabled" : "disabled";
    sendServerMessage("First person mode " + l_str_en + ".");
}
