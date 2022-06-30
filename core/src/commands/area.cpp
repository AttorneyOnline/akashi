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
#include "include/config_manager.h"
#include "include/packet/packet_factory.h"
#include "include/server.h"

// This file is for commands under the area category in aoclient.h
// Be sure to register the command in the header before adding it here!

void AOClient::cmdCM(int argc, QStringList argv)
{
    QString l_sender_name = m_ooc_name;
    AreaData *l_area = server->getAreaById(m_current_area);
    if (l_area->isProtected()) {
        sendServerMessage("This area is protected, you may not become CM.");
        return;
    }
    else if (l_area->owners().isEmpty()) { // no one owns this area, and it's not protected
        l_area->addOwner(m_id);
        sendServerMessageArea(l_sender_name + " is now CM in this area.");
        arup(ARUPType::CM, true);
    }
    else if (!l_area->owners().contains(m_id)) { // there is already a CM, and it isn't us
        sendServerMessage("You cannot become a CM in this area.");
    }
    else if (argc == 1) { // we are CM, and we want to make ID argv[0] also CM
        bool ok;
        AOClient *l_owner_candidate = server->getClientByID(argv[0].toInt(&ok));
        if (!ok) {
            sendServerMessage("That doesn't look like a valid ID.");
            return;
        }
        if (l_owner_candidate == nullptr) {
            sendServerMessage("Unable to find client with ID " + argv[0] + ".");
            return;
        }
        if (l_area->owners().contains(l_owner_candidate->m_id)) {
            sendServerMessage("User is already a CM in this area.");
            return;
        }
        l_area->addOwner(l_owner_candidate->m_id);
        sendServerMessageArea(l_owner_candidate->m_ooc_name + " is now CM in this area.");
        arup(ARUPType::CM, true);
    }
    else {
        sendServerMessage("You are already a CM in this area.");
    }
}

void AOClient::cmdUnCM(int argc, QStringList argv)
{
    AreaData *l_area = server->getAreaById(m_current_area);
    int l_uid;

    if (l_area->owners().isEmpty()) {
        sendServerMessage("There are no CMs in this area.");
        return;
    }
    else if (argc == 0) {
        l_uid = m_id;
        sendServerMessage("You are no longer CM in this area.");
    }
    else if (checkPermission(ACLRole::UNCM) && argc >= 1) {
        bool conv_ok = false;
        l_uid = argv[0].toInt(&conv_ok);
        if (!conv_ok) {
            sendServerMessage("Invalid user ID.");
            return;
        }
        if (!l_area->owners().contains(l_uid)) {
            sendServerMessage("That user is not CMed.");
            return;
        }
        AOClient *l_target = server->getClientByID(l_uid);
        if (l_target == nullptr) {
            sendServerMessage("No client with that ID found.");
            return;
        }
        sendServerMessage(l_target->m_ooc_name + " was successfully unCMed.");
        l_target->sendServerMessage("You have been unCMed by a moderator.");
    }
    else {
        sendServerMessage("You do not have permission to unCM others.");
        return;
    }

    if (l_area->removeOwner(l_uid)) {
        arup(ARUPType::LOCKED, true);
    }

    arup(ARUPType::CM, true);
}

void AOClient::cmdInvite(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    AreaData *l_area = server->getAreaById(m_current_area);
    bool ok;
    int l_invited_id = argv[0].toInt(&ok);
    if (!ok) {
        sendServerMessage("That does not look like a valid ID.");
        return;
    }

    AOClient *target_client = server->getClientByID(l_invited_id);
    if (target_client == nullptr) {
        sendServerMessage("No client with that ID found.");
        return;
    }
    else if (!l_area->invite(l_invited_id)) {
        sendServerMessage("That ID is already on the invite list.");
        return;
    }
    sendServerMessage("You invited ID " + argv[0]);
    target_client->sendServerMessage("You were invited and given access to " + l_area->name());
}

void AOClient::cmdUnInvite(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    AreaData *l_area = server->getAreaById(m_current_area);
    bool ok;
    int l_uninvited_id = argv[0].toInt(&ok);
    if (!ok) {
        sendServerMessage("That does not look like a valid ID.");
        return;
    }

    AOClient *target_client = server->getClientByID(l_uninvited_id);
    if (target_client == nullptr) {
        sendServerMessage("No client with that ID found.");
        return;
    }
    else if (l_area->owners().contains(l_uninvited_id)) {
        sendServerMessage("You cannot uninvite a CM!");
        return;
    }
    else if (!l_area->uninvite(l_uninvited_id)) {
        sendServerMessage("That ID is not on the invite list.");
        return;
    }
    sendServerMessage("You uninvited ID " + argv[0]);
    target_client->sendServerMessage("You were uninvited from " + l_area->name());
}

void AOClient::cmdLock(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    AreaData *area = server->getAreaById(m_current_area);
    if (area->lockStatus() == AreaData::LockStatus::LOCKED) {
        sendServerMessage("This area is already locked.");
        return;
    }
    sendServerMessageArea("This area is now locked.");
    area->lock();
    const QVector<AOClient *> l_clients = server->getClients();
    for (AOClient *l_client : l_clients) {
        if (l_client->m_current_area == m_current_area && l_client->hasJoined()) {
            area->invite(l_client->m_id);
        }
    }
    arup(ARUPType::LOCKED, true);
}

void AOClient::cmdSpectatable(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    AreaData *l_area = server->getAreaById(m_current_area);
    if (l_area->lockStatus() == AreaData::LockStatus::SPECTATABLE) {
        sendServerMessage("This area is already in spectate mode.");
        return;
    }
    sendServerMessageArea("This area is now spectatable.");
    l_area->spectatable();
    const QVector<AOClient *> l_clients = server->getClients();
    for (AOClient *l_client : l_clients) {
        if (l_client->m_current_area == m_current_area && l_client->hasJoined()) {
            l_area->invite(l_client->m_id);
        }
    }
    arup(ARUPType::LOCKED, true);
}

void AOClient::cmdUnLock(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    AreaData *l_area = server->getAreaById(m_current_area);
    if (l_area->lockStatus() == AreaData::LockStatus::FREE) {
        sendServerMessage("This area is not locked.");
        return;
    }
    sendServerMessageArea("This area is now unlocked.");
    l_area->unlock();
    arup(ARUPType::LOCKED, true);
}

void AOClient::cmdGetAreas(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QStringList l_entries;
    l_entries.append("\n== Currently Online: " + QString::number(server->getPlayerCount()) + " ==");
    for (int i = 0; i < server->getAreaCount(); i++) {
        if (server->getAreaById(i)->playerCount() > 0) {
            QStringList l_cur_area_lines = buildAreaList(i);
            l_entries.append(l_cur_area_lines);
        }
    }
    sendServerMessage(l_entries.join("\n"));
}

void AOClient::cmdGetArea(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QStringList l_entries = buildAreaList(m_current_area);
    sendServerMessage(l_entries.join("\n"));
}

void AOClient::cmdArea(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    bool ok;
    int l_new_area = argv[0].toInt(&ok);
    if (!ok || l_new_area >= server->getAreaCount() || l_new_area < 0) {
        sendServerMessage("That does not look like a valid area ID.");
        return;
    }
    changeArea(l_new_area);
}

void AOClient::cmdAreaKick(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    AreaData *l_area = server->getAreaById(m_current_area);

    bool ok;
    int l_idx = argv[0].toInt(&ok);
    if (!ok) {
        sendServerMessage("That does not look like a valid ID.");
        return;
    }
    if (server->getAreaById(m_current_area)->owners().contains(l_idx)) {
        sendServerMessage("You cannot kick another CM!");
        return;
    }
    AOClient *l_client_to_kick = server->getClientByID(l_idx);
    if (l_client_to_kick == nullptr) {
        sendServerMessage("No client with that ID found.");
        return;
    }
    else if (l_client_to_kick->m_current_area != m_current_area) {
        sendServerMessage("That client is not in this area.");
        return;
    }
    l_client_to_kick->changeArea(0);
    l_area->uninvite(l_client_to_kick->m_id);

    sendServerMessage("Client " + argv[0] + " kicked back to area 0.");
}

void AOClient::cmdSetBackground(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    QString f_background = argv.join(" ");
    AreaData *area = server->getAreaById(m_current_area);
    if (m_authenticated || !area->bgLocked()) {
        if (server->getBackgrounds().contains(f_background, Qt::CaseInsensitive) || area->ignoreBgList() == true) {
            area->setBackground(f_background);
            server->broadcast(PacketFactory::createPacket("BN", {f_background}), m_current_area);
            sendServerMessageArea(m_current_char + " changed the background to " + f_background);
        }
        else {
            sendServerMessage("Invalid background name.");
        }
    }
    else {
        sendServerMessage("This area's background is locked.");
    }
}

void AOClient::cmdBgLock(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    AreaData *l_area = server->getAreaById(m_current_area);

    if (l_area->bgLocked() == false) {
        l_area->toggleBgLock();
    };

    server->broadcast(PacketFactory::createPacket("CT", {ConfigManager::serverName(), m_current_char + " locked the background.", "1"}), m_current_area);
}

void AOClient::cmdBgUnlock(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    AreaData *l_area = server->getAreaById(m_current_area);

    if (l_area->bgLocked() == true) {
        l_area->toggleBgLock();
    };

    server->broadcast(PacketFactory::createPacket("CT", {ConfigManager::serverName(), m_current_char + " unlocked the background.", "1"}), m_current_area);
}

void AOClient::cmdStatus(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    AreaData *l_area = server->getAreaById(m_current_area);
    QString l_arg = argv[0].toLower();

    if (l_area->changeStatus(l_arg)) {
        arup(ARUPType::STATUS, true);
        server->broadcast(PacketFactory::createPacket("CT", {ConfigManager::serverName(), m_current_char + " changed status to " + l_arg.toUpper(), "1"}), m_current_area);
    }
    else {
        const QStringList keys = AreaData::map_statuses.keys();
        sendServerMessage("That does not look like a valid status. Valid statuses are " + keys.join(", "));
    }
}

void AOClient::cmdJudgeLog(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    AreaData *l_area = server->getAreaById(m_current_area);
    if (l_area->judgelog().isEmpty()) {
        sendServerMessage("There have been no judge actions in this area.");
        return;
    }
    QString l_message = l_area->judgelog().join("\n");
    // Judgelog contains an IPID, so we shouldn't send that unless the caller has appropriate permissions
    if (checkPermission(ACLRole::KICK) || checkPermission(ACLRole::BAN)) {
        sendServerMessage(l_message);
    }
    else {
        QString filteredmessage = l_message.remove(QRegularExpression("[(].*[)]")); // Filter out anything between two parentheses. This should only ever be the IPID
        sendServerMessage(filteredmessage);
    }
}

void AOClient::cmdIgnoreBgList(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    AreaData *l_area = server->getAreaById(m_current_area);
    l_area->toggleIgnoreBgList();
    QString l_state = l_area->ignoreBgList() ? "ignored." : "enforced.";
    sendServerMessage("BG list in this area is now " + l_state);
}

void AOClient::cmdAreaMessage(int argc, QStringList argv)
{
    AreaData *l_area = server->getAreaById(m_current_area);
    if (argc == 0) {
        sendServerMessage(l_area->areaMessage());
        return;
    }

    if (argc >= 1) {
        l_area->changeAreaMessage(argv.join(" "));
        sendServerMessage("Updated this area's message.");
    }
}

void AOClient::cmdToggleAreaMessageOnJoin(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    AreaData *l_area = server->getAreaById(m_current_area);
    l_area->toggleAreaMessageJoin();
    QString l_state = l_area->sendAreaMessageOnJoin() ? "enabled." : "disabled.";
    sendServerMessage("Sending message on area join is now " + l_state);
}

void AOClient::cmdToggleWtce(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    AreaData *l_area = server->getAreaById(m_current_area);
    l_area->toggleWtceAllowed();
    QString l_state = l_area->isWtceAllowed() ? "enabled." : "disabled.";
    sendServerMessage("Using testimony animations is now " + l_state);
}

void AOClient::cmdToggleShouts(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    AreaData *l_area = server->getAreaById(m_current_area);
    l_area->toggleShoutAllowed();
    QString l_state = l_area->isShoutAllowed() ? "enabled." : "disabled.";
    sendServerMessage("Using shouts is now " + l_state);
}

void AOClient::cmdClearAreaMessage(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    AreaData *l_area = server->getAreaById(m_current_area);
    l_area->clearAreaMessage();
    if (l_area->sendAreaMessageOnJoin())              // Turn off the automatic sending.
        cmdToggleAreaMessageOnJoin(0, QStringList{}); // Dummy values.
}
