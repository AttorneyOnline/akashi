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
#include "aoclient.h"

#include "area_data.h"
#include "config_manager.h"
#include "packet/packet_factory.h"
#include "server.h"

// This file is for commands under the area category in aoclient.h
// Be sure to register the command in the header before adding it here!

void AOClient::cmdCM(int argc, QStringList argv)
{
    const bool canOverride = checkPermission(ACLRole::KICK);
    if (argc > 2 || (argc == 2 && !canOverride)) {
        sendServerMessage("You do not have permission to specify an area.");
        return;
    }
    int targetAreaId = areaId();
    if (argc == 2) {
        bool ok = false;
        targetAreaId = argv[1].toInt(&ok);
        if (!ok || targetAreaId < 0 || targetAreaId >= server->getAreaCount()) {
            sendServerMessage("That does not look like a valid area ID.");
            return;
        }
    }
    AreaData *l_area = server->getAreaById(targetAreaId);
    if (l_area->isProtected() && !checkPermission(ACLRole::SUPER)) {
        sendServerMessage("This area is protected, you may not become CM in this area.");
        return;
    }

    AOClient *candidate = this;
    if (argc >= 1 && (canOverride || l_area->owners().contains(clientId()))) {
        bool ok = false;
        const int id = argv[0].toInt(&ok);
        if (!ok) {
            sendServerMessage("That doesn't look like a valid ID.");
            return;
        }
        candidate = server->getClientByID(id);
        if (!candidate) {
            sendServerMessage("Unable to find client with ID " + argv[0] + ".");
            return;
        }
    }
    else if (!l_area->owners().isEmpty() && !l_area->owners().contains(clientId()) && !canOverride) {
        sendServerMessage("You cannot become a CM in this area when someone else is. You must be CM'ed by an existing one.");
        return;
    }
    if (l_area->owners().contains(candidate->clientId())) {
        sendServerMessage(candidate == this ? "You are already a CM in this area." : "User is already a CM in this area.");
        return;
    }
    if (candidate->getIpid().isEmpty()) {
        sendServerMessage("That client is not valid.");
        return;
    }
    l_area->addOwner(candidate->clientId(), candidate->getIpid());
    const QString message = candidate->name() + " is now CM in " + l_area->displayName() + ".";
    server->broadcast(PacketFactory::createPacket("CT", {ConfigManager::serverNickname(), message, "1"}), targetAreaId);
    if (targetAreaId != areaId())
        sendServerMessage(message);
    arup(ARUPType::CM, true);
}

void AOClient::cmdUnCM(int argc, QStringList argv)
{
    const bool canKick = checkPermission(ACLRole::KICK);
    if (argc > 2) {
        sendServerMessage("Use /uncm [user ID] [area ID].");
        return;
    }
    int targetAreaId = areaId();
    if (argc == 2) {
        if (!canKick) {
            sendServerMessage("You do not have permission to specify an area.");
            return;
        }
        bool ok = false;
        targetAreaId = argv[1].toInt(&ok);
        if (!ok || targetAreaId < 0 || targetAreaId >= server->getAreaCount()) {
            sendServerMessage("That does not look like a valid area ID.");
            return;
        }
        if (argv[0] == "all") {
            sendServerMessage("Specify a user ID when selecting an area.");
            return;
        }
    }
    AreaData *l_area = server->getAreaById(targetAreaId);
    int l_uid;

    if (l_area->owners().isEmpty()) {
        sendServerMessage("There are no CMs in this area.");
        return;
    }
    else if (argc == 0) {
        l_uid = clientId();
        if (!l_area->owners().contains(l_uid)) {
            sendServerMessage("You are not a CM in this area.");
            return;
        }
        sendServerMessage("You are no longer CM in this area.");
    }
    else if (checkPermission(ACLRole::UNCM) || canKick) {

        // Remove all owners except yourself
        if (argv[0] == "all") {
            if (!checkPermission(ACLRole::UNCM)) {
                sendServerMessage("You do not have permission to unCM all clients.");
                return;
            }
            QList<int> owners = l_area->owners();
            for (int uid : owners) {
                if (uid != clientId()) {
                    l_area->removeOwner(uid);
                    AOClient *l_target = server->getClientByID(uid);
                    if (l_target != nullptr) {
                        l_target->sendServerMessage("You have been unCMed.");
                    }
                }
            }
            sendServerMessage("All CMs except yourself have been unCMed.");
            arup(ARUPType::LOCKED, true);
            arup(ARUPType::CM, true);
            return;
        }

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
        if (targetAreaId == areaId()) {
            sendServerMessage(l_target->name() + " was successfully unCMed.");
            l_target->sendServerMessage("You have been unCMed by a moderator.");
        }
        else {
            sendServerMessage(l_target->name() + " was successfully unCMed in " + l_area->displayName() + ".");
            l_target->sendServerMessage("You have been unCMed in " + l_area->displayName() + " by a moderator.");
        }
    }
    else {
        sendServerMessage("You do not have permission to unCM others. Only yourself.");
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

    AreaData *l_area = server->getAreaById(areaId());
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
    if (target_client->getIpid().isEmpty()) {
        sendServerMessage("That client's IPID is unavailable.");
        return;
    }
    if (!l_area->invite(target_client->getIpid())) {
        sendServerMessage("That client's IPID is already on the invite list.");
        return;
    }
    sendServerMessage("You invited ID " + argv[0]);
    target_client->sendServerMessage("You were invited and given access to " + l_area->name());
}

void AOClient::cmdUnInvite(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    AreaData *l_area = server->getAreaById(areaId());
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
    for (int ownerId : l_area->owners()) {
        AOClient *owner = server->getClientByID(ownerId);
        if (owner && owner->getIpid() == target_client->getIpid()) {
            sendServerMessage("You cannot uninvite a CM's IPID!");
            return;
        }
    }
    if (!l_area->uninvite(target_client->getIpid())) {
        sendServerMessage("That ID is not on the invite list.");
        return;
    }
    sendServerMessage("You uninvited ID " + argv[0]);
    target_client->sendServerMessage("You were uninvited from " + l_area->name());
}

void AOClient::cmdInviteList(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    AreaData *area = server->getAreaById(areaId());
    QStringList lines;
    lines.append("=== Invites for " + area->displayName() + " ===");
    for (const QString &ipid : area->invited()) {
        const QList<AOClient *> clients = server->getClientsByIpid(ipid);
        if (clients.isEmpty()) {
            lines.append("[offline] " + ipid);
            continue;
        }
        for (const AOClient *client : clients)
            lines.append("[" + QString::number(client->clientId()) + "] " + client->character() + " (" + ipid + ")");
    }
    if (area->invited().isEmpty())
        lines.append("No invited IPIDs.");
    sendServerMessage(lines.join("\n"));
}

void AOClient::cmdLock(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    AreaData *area = server->getAreaById(areaId());
    if (!canUseAreaControl(area->lockingAllowed())) {
        sendServerMessage("You do not have permission to lock this area.");
        return;
    }
    if (area->lockStatus() == AreaData::LockStatus::LOCKED) {
        sendServerMessage("This area is already locked.");
        return;
    }
    sendServerMessageArea("This area is now locked.");
    area->lock();
    const QVector<AOClient *> l_clients = server->getClients();
    for (AOClient *l_client : l_clients) {
        if ((l_client->areaId() == areaId() && l_client->hasJoined()) || area->owners().contains(l_client->clientId())) {
            area->invite(l_client->getIpid());
        }
    }
    arup(ARUPType::LOCKED, true);
}

void AOClient::cmdSpectatable(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    AreaData *l_area = server->getAreaById(areaId());
    if (!canUseAreaControl(l_area->lockingAllowed())) {
        sendServerMessage("You do not have permission to lock this area.");
        return;
    }
    if (l_area->lockStatus() == AreaData::LockStatus::SPECTATABLE) {
        sendServerMessage("This area is already in spectate mode.");
        return;
    }
    sendServerMessageArea("This area is now spectatable.");
    l_area->spectatable();
    const QVector<AOClient *> l_clients = server->getClients();
    for (AOClient *l_client : l_clients) {
        if ((l_client->areaId() == areaId() && l_client->hasJoined()) || l_area->owners().contains(l_client->clientId())) {
            l_area->invite(l_client->getIpid());
        }
    }
    arup(ARUPType::LOCKED, true);
}

void AOClient::cmdToggleLocking(int argc, QStringList argv)
{
    if (!checkPermission(ACLRole::SUPER)) {
        sendServerMessage("You do not have permission to use that command.");
        return;
    }
    if (argc != 1 || !server->getAreaById(areaId())->setLockingAllowed(argv[0])) {
        sendServerMessage("Use /togglelocking <SUPER|Mod|CM>.");
        return;
    }
    sendServerMessage("Area locking access set to " + argv[0].toUpper() + ".");
}

void AOClient::cmdToggleStreaming(int argc, QStringList argv)
{
    if (!checkPermission(ACLRole::SUPER)) {
        sendServerMessage("You do not have permission to use that command.");
        return;
    }
    if (argc != 1 || !server->getAreaById(areaId())->setPlayStreamingAllowed(argv[0])) {
        sendServerMessage("Use /togglestreaming <SUPER|Mod|CM|FFA>.");
        return;
    }
    sendServerMessage("Area play command access set to " + argv[0].toUpper() + ".");
}

void AOClient::cmdUnLock(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    AreaData *l_area = server->getAreaById(areaId());
    if (l_area->lockStatus() == AreaData::LockStatus::FREE) {
        if (l_area->invited().isEmpty()) {
            sendServerMessage("This area is not locked.");
        }
        else {
            l_area->unlock();
            sendServerMessage("This area is already unlocked; its invite list has been cleared.");
        }
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

    QStringList l_entries = buildAreaList(areaId());
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
    AreaData *l_area = server->getAreaById(areaId());
    const auto uninviteUnlessCM = [this, l_area](AOClient *client) {
        for (int ownerId : l_area->owners()) {
            AOClient *owner = server->getClientByID(ownerId);
            if (owner && owner->getIpid() == client->getIpid())
                return;
        }
        l_area->uninvite(client->getIpid());
    };

    int target_area_id = 0; // Default to first area of the server

    // Check if a target area is provided
    if (argc >= 2) {
        if (!checkPermission(ACLRole::KICK)) {
            sendServerMessage("You do not have permission to kick to specific areas. Just the first area as CM. (/areakick [ID]).");
            return;
        }

        bool ok;
        target_area_id = argv[1].toInt(&ok);
        if (!ok || target_area_id < 0 || target_area_id >= server->getAreaCount()) {
            sendServerMessage("That does not look like a valid area ID.");
            return;
        }
    }

    AreaData *target_area = server->getAreaById(target_area_id);

    if (argv[0] == "all") {
        const QVector<AOClient *> l_clients = server->getClients();
        for (AOClient *l_client : l_clients) {
            if (l_client->areaId() == areaId() && l_client->clientId() != clientId()) {
                if (!server->getAreaById(areaId())->owners().contains(l_client->clientId())) {
                    l_client->changeArea(target_area_id);
                    uninviteUnlessCM(l_client);
                    l_client->sendServerMessage("You have been kicked to area " + target_area->displayName() + ".");
                }
            }
        }
        sendServerMessage("All clients kicked to area " + target_area->displayName() + ".");
        return;
    }

    // Without secondary area argument
    bool ok;
    int l_idx = argv[0].toInt(&ok);
    if (!ok) {
        sendServerMessage("That does not look like a valid ID.");
        return;
    }
    if (server->getAreaById(areaId())->owners().contains(l_idx)) {
        sendServerMessage("You cannot kick another CM!");
        return;
    }
    AOClient *l_client_to_kick = server->getClientByID(l_idx);
    if (l_client_to_kick == nullptr) {
        sendServerMessage("No client with that ID found.");
        return;
    }
    else if (l_client_to_kick->areaId() != areaId()) {
        sendServerMessage("That client is not in this area.");
        return;
    }
    l_client_to_kick->changeArea(target_area_id);
    uninviteUnlessCM(l_client_to_kick);
    l_client_to_kick->sendServerMessage("You have been kicked to area " + target_area->displayName() + ".");
    sendServerMessage("Client " + argv[0] + " kicked to area " + target_area->displayName() + ".");
}

void AOClient::cmdSetBackground(int argc, QStringList argv)
{
    AreaData *area = server->getAreaById(areaId());
    if (argc == 0) {
        sendServerMessage("The current background is " + area->background() + ".");
        return;
    }
    QString f_background = argv.join(" ");
    if (m_authenticated || !area->bgLocked()) {
        if (area->lockStatus() == AreaData::LockStatus::SPECTATABLE && !area->invited().contains(m_ipid) && !checkPermission(ACLRole::BYPASS_LOCKS)) {
            sendServerMessage("Spectators are blocked from changing the background.");
            return;
        }
        if (server->getBackgrounds().contains(f_background, Qt::CaseInsensitive) || area->ignoreBgList() == true) {
            area->setBackground(f_background);
            server->broadcast(PacketFactory::createPacket("BN", {f_background, area->side()}), areaId());
            QString ambience_name = ConfigManager::ambience()->value(f_background + "/ambience").toString();
            if (ambience_name != "") {
                server->broadcast(PacketFactory::createPacket("MC", {ambience_name, "-1", characterName(), "1", "1"}), areaId());
            }
            else {
                server->broadcast(PacketFactory::createPacket("MC", {"~stop.mp3", "-1", characterName(), "1", "1"}), areaId());
            }
            sendServerMessageArea(character() + " changed the background to " + f_background);
        }
        else {
            sendServerMessage("Invalid background name.");
        }
    }
    else {
        sendServerMessage("This area's background is locked.");
    }
}

void AOClient::cmdSetSide(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    AreaData *area = server->getAreaById(areaId());
    if (area->bgLocked()) {
        sendServerMessage("This area's background is locked.");
        return;
    }

    QString side = argv.join(" ");
    area->setSide(side);
    server->broadcast(PacketFactory::createPacket("BN", {area->background(), side}), areaId());
    if (side.isEmpty()) {
        sendServerMessageArea(character() + " unlocked the background side");
    }
    else {
        sendServerMessageArea(character() + " locked the background side to " + side);
    }
}

void AOClient::cmdBgLock(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    AreaData *l_area = server->getAreaById(areaId());

    if (l_area->bgLocked() == false) {
        l_area->toggleBgLock();
    };

    server->broadcast(PacketFactory::createPacket("CT", {ConfigManager::serverNickname(), character() + " locked the background.", "1"}), areaId());
}

void AOClient::cmdBgUnlock(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    AreaData *l_area = server->getAreaById(areaId());

    if (l_area->bgLocked() == true) {
        l_area->toggleBgLock();
    };

    server->broadcast(PacketFactory::createPacket("CT", {ConfigManager::serverNickname(), character() + " unlocked the background.", "1"}), areaId());
}

void AOClient::cmdStatus(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    AreaData *l_area = server->getAreaById(areaId());
    QString l_arg = argv[0].toLower();

    if (l_area->changeStatus(l_arg)) {
        arup(ARUPType::STATUS, true);
        server->broadcast(PacketFactory::createPacket("CT", {ConfigManager::serverNickname(), character() + " changed status to " + l_arg.toUpper(), "1"}), areaId());
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

    AreaData *l_area = server->getAreaById(areaId());
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

    AreaData *l_area = server->getAreaById(areaId());
    l_area->toggleIgnoreBgList();
    QString l_state = l_area->ignoreBgList() ? "ignored." : "enforced.";
    sendServerMessage("BG list in this area is now " + l_state);
}

void AOClient::cmdAreaMessage(int argc, QStringList argv)
{
    AreaData *l_area = server->getAreaById(areaId());
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

    AreaData *l_area = server->getAreaById(areaId());
    l_area->toggleAreaMessageJoin();
    QString l_state = l_area->sendAreaMessageOnJoin() ? "enabled." : "disabled.";
    sendServerMessage("Sending message on area join is now " + l_state);
}

void AOClient::cmdToggleWtce(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    AreaData *l_area = server->getAreaById(areaId());
    l_area->toggleWtceAllowed();
    QString l_state = l_area->isWtceAllowed() ? "enabled." : "disabled.";
    sendServerMessage("Using testimony animations is now " + l_state);
}

void AOClient::cmdToggleShouts(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    AreaData *l_area = server->getAreaById(areaId());
    l_area->toggleShoutAllowed();
    QString l_state = l_area->isShoutAllowed() ? "enabled." : "disabled.";
    sendServerMessage("Using shouts is now " + l_state);
}

void AOClient::cmdClearAreaMessage(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    AreaData *l_area = server->getAreaById(areaId());
    l_area->clearAreaMessage();
    if (l_area->sendAreaMessageOnJoin()) // Turn off the automatic sending.
    {
        cmdToggleAreaMessageOnJoin(0, QStringList{}); // Dummy values.
    }
}

void AOClient::cmdWebfiles(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    const QVector<AOClient *> l_clients = server->getClients();
    QStringList l_weblinks;
    for (AOClient *l_client : l_clients) {
        if (l_client->m_current_iniswap.isEmpty() || l_client->areaId() != areaId()) {
            continue;
        }

        if (l_client->character().toLower() != l_client->m_current_iniswap.toLower()) {
            l_weblinks.append("https://attorneyonline.github.io/webDownloader/index.html?char=" + l_client->m_current_iniswap);
        }
    }
    sendServerMessage("Character files:\n" + l_weblinks.join("\n"));
}

void AOClient::cmdMedievalMode(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    AreaData *l_area = server->getAreaById(areaId());
    l_area->toggleMedievalMode();
    QString l_state = l_area->isMedievalMode() ? "enabled." : "disabled.";
    sendServerMessageArea("Hear ye, hear ye! Medieval Mode is now " + l_state);
}
