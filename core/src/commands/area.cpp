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

// This file is for commands under the area category in aoclient.h
// Be sure to register the command in the header before adding it here!


void AOClient::cmdCM(int argc, QStringList argv)
{
    QString sender_name = ooc_name;
    AreaData* area = server->areas[current_area];
    if (area->isProtected()) {
        sendServerMessage("This area is protected, you may not become CM.");
        return;
    }
    else if (area->owners().isEmpty()) { // no one owns this area, and it's not protected
        area->addOwner(id);
        sendServerMessageArea(sender_name + " is now CM in this area.");
        arup(ARUPType::CM, true);
    }
    else if (!area->owners().contains(id)) { // there is already a CM, and it isn't us
        sendServerMessage("You cannot become a CM in this area.");
    }
    else if (argc == 1) { // we are CM, and we want to make ID argv[0] also CM
        bool ok;
        AOClient* owner_candidate = server->getClientByID(argv[0].toInt(&ok));
        if (!ok) {
            sendServerMessage("That doesn't look like a valid ID.");
            return;
        }
        if (owner_candidate == nullptr) {
            sendServerMessage("Unable to find client with ID " + argv[0] + ".");
            return;
        }
        area->addOwner(owner_candidate->id);
        sendServerMessageArea(owner_candidate->ooc_name + " is now CM in this area.");
        arup(ARUPType::CM, true);
    }
    else {
        sendServerMessage("You are already a CM in this area.");
    }
}

void AOClient::cmdUnCM(int argc, QStringList argv)
{
    AreaData* area = server->areas[current_area];
    int uid;

    if (area->owners().isEmpty()) {
        sendServerMessage("There are no CMs in this area.");
        return;
    }
    else if (argc == 0) {
        uid = id;
        sendServerMessage("You are no longer CM in this area.");
    }
    else if (checkAuth(ACLFlags.value("UNCM")) && argc == 1) {
        bool conv_ok = false;
        uid = argv[0].toInt(&conv_ok);
        if (!conv_ok) {
            sendServerMessage("Invalid user ID.");
            return;
        }
        if (!area->owners().contains(uid)) {
            sendServerMessage("That user is not CMed.");
            return;
        }
        AOClient* target = server->getClientByID(uid);
        target->sendServerMessage("You have been unCMed by a moderator.");
    }
    else {
        sendServerMessage("Invalid command.");
        return;
    }

    if (area->removeOwner(uid)) {
        arup(ARUPType::LOCKED, true);
    }

    arup(ARUPType::CM, true);
}

void AOClient::cmdInvite(int argc, QStringList argv)
{
    AreaData* area = server->areas[current_area];
    bool ok;
    int invited_id = argv[0].toInt(&ok);
    if (!ok) {
        sendServerMessage("That does not look like a valid ID.");
        return;
    }
    else if (server->getClientByID(invited_id) == nullptr) {
        sendServerMessage("No client with that ID found.");
        return;
    }
    else if (!area->invite(invited_id)) {
        sendServerMessage("That ID is already on the invite list.");
        return;
    }
    sendServerMessage("You invited ID " + argv[0]);
}

void AOClient::cmdUnInvite(int argc, QStringList argv)
{
    AreaData* area = server->areas[current_area];
    bool ok;
    int uninvited_id = argv[0].toInt(&ok);
    if (!ok) {
        sendServerMessage("That does not look like a valid ID.");
        return;
    }
    else if (server->getClientByID(uninvited_id) == nullptr) {
        sendServerMessage("No client with that ID found.");
        return;
    }
    else if (area->owners().contains(uninvited_id)) {
        sendServerMessage("You cannot uninvite a CM!");
        return;
    }
    else if (!area->invited().contains(uninvited_id)) {
        sendServerMessage("That ID is not on the invite list.");
        return;
    }
    area->invited().removeAll(uninvited_id);
    sendServerMessage("You uninvited ID " + argv[0]);
}

void AOClient::cmdLock(int argc, QStringList argv)
{
    AreaData* area = server->areas[current_area];
    if (area->lockStatus() == AreaData::LockStatus::LOCKED) {
        sendServerMessage("This area is already locked.");
        return;
    }
    sendServerMessageArea("This area is now locked.");
    area->lock();
    for (AOClient* client : server->clients) {
        if (client->current_area == current_area && client->joined) {
            area->invited().append(client->id);
        }
    }
    arup(ARUPType::LOCKED, true);
}

void AOClient::cmdSpectatable(int argc, QStringList argv)
{
    AreaData* area = server->areas[current_area];
    if (area->lockStatus() == AreaData::LockStatus::SPECTATABLE) {
        sendServerMessage("This area is already in spectate mode.");
        return;
    }
    sendServerMessageArea("This area is now spectatable.");
    area->spectatable();
    for (AOClient* client : server->clients) {
        if (client->current_area == current_area && client->joined) {
            area->invited().append(client->id);
        }
    }
    arup(ARUPType::LOCKED, true);
}

void AOClient::cmdUnLock(int argc, QStringList argv)
{
    AreaData* area = server->areas[current_area];
    if (area->lockStatus() == AreaData::LockStatus::FREE) {
        sendServerMessage("This area is not locked.");
        return;
    }
    sendServerMessageArea("This area is now unlocked.");
    area->unlock();
    arup(ARUPType::LOCKED, true);
}

void AOClient::cmdGetAreas(int argc, QStringList argv)
{
    QStringList entries;
    entries.append("== Area List ==");
    for (int i = 0; i < server->area_names.length(); i++) {
        QStringList cur_area_lines = buildAreaList(i);
        entries.append(cur_area_lines);
    }
    sendServerMessage(entries.join("\n"));
}

void AOClient::cmdGetArea(int argc, QStringList argv)
{
    QStringList entries = buildAreaList(current_area);
    sendServerMessage(entries.join("\n"));
}

void AOClient::cmdArea(int argc, QStringList argv)
{
    bool ok;
    int new_area = argv[0].toInt(&ok);
    if (!ok || new_area >= server->areas.size() || new_area < 0) {
        sendServerMessage("That does not look like a valid area ID.");
        return;
    }
    changeArea(new_area);
}

void AOClient::cmdAreaKick(int argc, QStringList argv)
{
    bool ok;
    int idx = argv[0].toInt(&ok);
    if (!ok) {
        sendServerMessage("That does not look like a valid ID.");
        return;
    }
    AOClient* client_to_kick = server->getClientByID(idx);
    if (client_to_kick == nullptr) {
        sendServerMessage("No client with that ID found.");
        return;
    }
    client_to_kick->changeArea(0);
    sendServerMessage("Client " + argv[0] + " kicked back to area 0.");
}

void AOClient::cmdSetBackground(int argc, QStringList argv)
{
    AreaData* area = server->areas[current_area];
    if (authenticated || !area->bgLocked()) {
        if (server->backgrounds.contains(argv[0])) {
            area->background() = argv[0];
            server->broadcast(AOPacket("BN", {argv[0]}), current_area);
            sendServerMessageArea(current_char + " changed the background to " + argv[0]);
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
    AreaData* area = server->areas[current_area];

    if (area->bgLocked() == false) {
        area->toggleBgLock();
    };

    server->broadcast(AOPacket("CT", {"Server", current_char + " locked the background.", "1"}), current_area);
}

void AOClient::cmdBgUnlock(int argc, QStringList argv)
{
    AreaData* area = server->areas[current_area];

    if (area->bgLocked() == true) {
        area->toggleBgLock();
    };

    server->broadcast(AOPacket("CT", {"Server", current_char + " unlocked the background.", "1"}), current_area);
}

void AOClient::cmdStatus(int argc, QStringList argv)
{
    AreaData* area = server->areas[current_area];
    QString arg = argv[0].toLower();

    if (area->changeStatus(arg)) {
        arup(ARUPType::STATUS, true);
        sendServerMessageArea(ooc_name + " changed status to " + arg);
    } else {
        sendServerMessage("That does not look like a valid status. Valid statuses are " + AreaData::map_statuses.keys().join(", "));
        return;
    }
}

void AOClient::cmdJudgeLog(int argc, QStringList argv)
{
    AreaData* area = server->areas[current_area];
    if (area->judgelog().isEmpty()) {
        sendServerMessage("There have been no judge actions in this area.");
        return;
    }
    QString message = area->judgelog().join("\n");
    //Judgelog contains an IPID, so we shouldn't send that unless the caller has appropriate permissions
    if (checkAuth(ACLFlags.value("KICK")) == 1 || checkAuth(ACLFlags.value("BAN")) == 1) {
            sendServerMessage(message);
    }
    else {
        QString filteredmessage = message.remove(QRegularExpression("[(].*[)]")); //Filter out anything between two parentheses. This should only ever be the IPID
        sendServerMessage(filteredmessage);
    }
}
