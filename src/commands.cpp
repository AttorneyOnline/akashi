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
//    but WITHOUT ANY WARRANTY; without even the implied warranty of                //
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                 //
//    GNU Affero General Public License for more details.                           //
//                                                                                  //
//    You should have received a copy of the GNU Affero General Public License      //
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.        //
//////////////////////////////////////////////////////////////////////////////////////
#include "include/aoclient.h"

// Be sure to register the command in the header before adding it here!

void AOClient::cmdDefault(int argc, QStringList argv)
{
    sendServerMessage("Invalid command.");
    return;
}

void AOClient::cmdLogin(int argc, QStringList argv)
{
    QSettings config("config/config.ini", QSettings::IniFormat);
    config.beginGroup("Options");
    QString modpass = config.value("modpass", "default").toString();
    QString auth_type = config.value("auth", "simple").toString();

    if (authenticated) {
        sendServerMessage("You are already logged in!");
        return;
    }

    if (auth_type == "simple") {
        if (modpass == "") {
            sendServerMessage("No modpass is set! Please set a modpass before authenticating.");
        }
        else if(argv[0] == modpass) {
            sendPacket("AUTH", {"1"}); // Client: "You were granted the Disable Modcalls button."
            sendServerMessage("Logged in as a moderator."); // for old clients, this is hardcoded to display the mod UI
            authenticated = true;
        } 
        else {
            sendPacket("AUTH", {"0"}); // Client: "Login unsuccessful."
            sendServerMessage("Incorrect password.");
        }
        server->areas.value(current_area)->logger->logLogin(this, authenticated, "moderator");
    }
    else {
        if (argc < 2) {
            sendServerMessage("You must specify a username and a password");
            return;
        }
        QString username = argv[0];
        QString password = argv[1];
        if (server->db_manager->authenticate(username, password)) {
            moderator_name = username;
            authenticated = true;
            sendPacket("AUTH", {"1"}); // Client: "You were granted the Disable Modcalls button."
            sendServerMessage("Logged in as a moderator."); // for old clients, this is hardcoded to display the mod UI
            sendServerMessage("Welcome, " + username);
        }
        else {
            sendPacket("AUTH", {"0"}); // Client: "Login unsuccessful."
            sendServerMessage("Incorrect password.");
        }
        server->areas.value(current_area)->logger->logLogin(this, authenticated, username);
    }
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

void AOClient::cmdBan(int argc, QStringList argv)
{
    QString target_ipid = argv[0];
    QHostAddress ip;
    QString hdid;
    unsigned long time = QDateTime::currentDateTime().toTime_t();
    QString reason = argv[1];
    bool ban_logged = false;

    if (argc > 2) {
        for (int i = 2; i < argv.length(); i++) {
            reason += " " + argv[i];
        }
    }

    for (AOClient* client : server->clients) {
        if (client->getIpid() == target_ipid) {
            if (!ban_logged) {
                ip = client->remote_ip;
                hdid = client->hwid;
                server->db_manager->addBan(target_ipid, ip, hdid, time, reason);
                sendServerMessage("Banned user with ipid " + target_ipid + " for reason: " + reason);
                ban_logged = true;
            }
            client->sendPacket("KB", {reason});
            client->socket->close();
        }
    }

    if (!ban_logged)
        sendServerMessage("User with ipid not found!");
}

void AOClient::cmdKick(int argc, QStringList argv)
{
    QString target_ipid = argv[0];
    QString reason = argv[1];
    bool did_kick = false;

    if (argc > 2) {
        for (int i = 2; i < argv.length(); i++) {
            reason += " " + argv[i];
        }
    }

    for (AOClient* client : server->clients) {
        if (client->getIpid() == target_ipid) {
            client->sendPacket("KK", {reason});
            client->socket->close();
            did_kick = true;
        }
    }

    if (did_kick)
        sendServerMessage("Kicked user with ipid " + target_ipid + " for reason: " + reason);
    else
        sendServerMessage("User with ipid not found!");
}

void AOClient::cmdChangeAuth(int argc, QStringList argv)
{
    QSettings settings("config/config.ini", QSettings::IniFormat);
    settings.beginGroup("Options");
    QString auth_type = settings.value("auth", "simple").toString();

    if (auth_type == "simple") {
        change_auth_started = true;
        sendServerMessage("WARNING!\nThis command will change how logging in as a moderator works.\nOnly proceed if you know what you are doing\nUse the command /rootpass to set the password for your root account.");
    }
}

void AOClient::cmdSetRootPass(int argc, QStringList argv)
{
    if (!change_auth_started)
        return;

    sendServerMessage("Changing auth type and setting root password.\nLogin again with /login root [password]");
    authenticated = false;
    QSettings settings("config/config.ini", QSettings::IniFormat);
    settings.beginGroup("Options");
    settings.setValue("auth", "advanced");

#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
    qsrand(QDateTime::currentMSecsSinceEpoch());
    quint32 upper_salt = qrand();
    quint32 lower_salt = qrand();
    quint64 salt_number = (upper_salt << 32) | lower_salt;
#else
    quint64 salt_number = QRandomGenerator::system()->generate64();
#endif
    QString salt = QStringLiteral("%1").arg(salt_number, 16, 16, QLatin1Char('0'));

    server->db_manager->createUser("root", salt, argv[0], ACLFlags.value("SUPER"));
}

void AOClient::cmdSetBackground(int argc, QStringList argv)
{
    AreaData* area = server->areas[current_area];
    if (authenticated || !area->bg_locked) {
        if (server->backgrounds.contains(argv[0])) {
            area->background = argv[0];
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
    area->bg_locked = true;
    server->broadcast(AOPacket("CT", {"Server", current_char + " locked the background.", "1"}), current_area);
}

void AOClient::cmdBgUnlock(int argc, QStringList argv)
{
    AreaData* area = server->areas[current_area];
    area->bg_locked = false;
    server->broadcast(AOPacket("CT", {"Server", current_char + " unlocked the background.", "1"}), current_area);
}

void AOClient::cmdAddUser(int argc, QStringList argv)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
    qsrand(QDateTime::currentMSecsSinceEpoch());
    quint32 upper_salt = qrand();
    quint32 lower_salt = qrand();
    quint64 salt_number = (upper_salt << 32) | lower_salt;
#else
    quint64 salt_number = QRandomGenerator::system()->generate64();
#endif
    QString salt = QStringLiteral("%1").arg(salt_number, 16, 16, QLatin1Char('0'));

    if (server->db_manager->createUser(argv[0], salt, argv[1], ACLFlags.value("NONE")))
        sendServerMessage("Created user " + argv[0] + ".\nUse /addperm to modify their permissions.");
    else
        sendServerMessage("Unable to create user " + argv[0] + ".\nDoes a user with that name already exist?");
}

void AOClient::cmdListPerms(int argc, QStringList argv)
{
    unsigned long long user_acl = server->db_manager->getACL(moderator_name);
    QStringList message;
    if (argc == 0) {
        // Just print out all permissions available to the user.
        message.append("You have been given the following permissions:");
        for (QString perm : ACLFlags.keys()) {
            if (perm == "NONE"); // don't need to list this one
            else if (perm == "SUPER") {
                if (user_acl == ACLFlags.value("SUPER")) // This has to be checked separately, because SUPER & anything will always be truthy
                    message.append("SUPER (Be careful! This grants the user all permissions.)");
            }
            else if ((ACLFlags.value(perm) & user_acl) == 0); // user doesn't have this permission, don't print it
            else
                message.append(perm);
        }
    }
    else {
        if ((user_acl & ACLFlags.value("MODIFY_USERS")) == 0) {
            sendServerMessage("You do not have permission to view other users' permissions.");
            return;
        }

        message.append("User " + argv[0] + " has the following permissions:");
        unsigned long long acl = server->db_manager->getACL(argv[0]);
        if (acl == 0) {
            sendServerMessage("This user either doesn't exist, or has no permissions set.");
            return;
        }

        for (QString perm : ACLFlags.keys()) {
            if ((ACLFlags.value(perm) & acl) != 0 && perm != "SUPER") {
                message.append(perm);
            }
        }
    }
    sendServerMessage(message.join("\n"));
}

void AOClient::cmdAddPerms(int argc, QStringList argv)
{
    unsigned long long user_acl = server->db_manager->getACL(moderator_name);
    argv[1] = argv[1].toUpper();

    if (!ACLFlags.keys().contains(argv[1])) {
        sendServerMessage("That permission doesn't exist!");
        return;
    }

    if (argv[1] == "SUPER") {
        if (user_acl != ACLFlags.value("SUPER")) {
            // This has to be checked separately, because SUPER & anything will always be truthy
            sendServerMessage("You aren't allowed to add that permission!");
            return;
        }
    }
    if (argv[1] == "NONE") {
        sendServerMessage("Added no permissions!");
        return;
    }

    unsigned long long newperm = ACLFlags.value(argv[1]);
    if ((newperm & user_acl) != 0) {
        if (server->db_manager->updateACL(argv[0], newperm, true))
            sendServerMessage("Successfully added permission " + argv[1] + " to user " + argv[0]);
        else
            sendServerMessage(argv[0] + " wasn't found!");
        return;
    }

    sendServerMessage("You aren't allowed to add that permission!");
}

void AOClient::cmdRemovePerms(int argc, QStringList argv)
{
    unsigned long long user_acl = server->db_manager->getACL(moderator_name);
    argv[1] = argv[1].toUpper();

    if (!ACLFlags.keys().contains(argv[1])) {
        sendServerMessage("That permission doesn't exist!");
        return;
    }

    if (argv[0] == "root") {
        sendServerMessage("You cannot change the permissions of the root account!");
        return;
    }

    if (argv[1] == "SUPER") {
        if (user_acl != ACLFlags.value("SUPER")) {
            // This has to be checked separately, because SUPER & anything will always be truthy
            sendServerMessage("You aren't allowed to remove that permission!");
            return;
        }
    }
    if (argv[1] == "NONE") {
        sendServerMessage("Removed no permissions!");
        return;
    }

    unsigned long long newperm = ACLFlags.value(argv[1]);
    if ((newperm & user_acl) != 0) {
        if (server->db_manager->updateACL(argv[0], newperm, false))
            sendServerMessage("Successfully removed permission " + argv[1] + " from user " + argv[0]);
        else
            sendServerMessage(argv[0] + " wasn't found!");
        return;
    }

    sendServerMessage("You aren't allowed to remove that permission!");
}

void AOClient::cmdListUsers(int argc, QStringList argv)
{
    QStringList users = server->db_manager->getUsers();
    sendServerMessage("All users:\n" + users.join("\n"));
}

void AOClient::cmdLogout(int argc, QStringList argv)
{
    if (!authenticated) {
        sendServerMessage("You are not logged in!");
        return;
    }
    authenticated = false;
    moderator_name = "";
    sendPacket("AUTH", {"-1"}); // Client: "You were logged out."
}

void AOClient::cmdPos(int argc, QStringList argv)
{
    pos = argv[0];
    sendServerMessage("Position changed to " + pos + ".");
}

void AOClient::cmdG(int argc, QStringList argv)
{
    QString sender_name = ooc_name;
    QString sender_area = server->area_names.value(current_area);
    QString sender_message = argv.join(" ");
    server->broadcast(AOPacket("CT", {"[" + sender_area + "]" + sender_name, sender_message}));
    return;
}

void AOClient::cmdNeed(int argc, QStringList argv)
{
    QString sender_area = server->area_names.value(current_area);
    QString sender_message = argv.join(" ");
    sendServerBroadcast({"=== Advert ===\n[" + sender_area + "] needs " + sender_message+ "."});
}

void AOClient::cmdFlip(int argc, QStringList argv)
{
    QString sender_name = ooc_name;
    QStringList faces = {"heads","tails"};
    QString face = faces[AOClient::genRand(0,1)];
    sendServerMessage(sender_name + " flipped a coin and got " + face + ".");
}

void AOClient::cmdRoll(int argc, QStringList argv)
{
    diceThrower(argc, argv, RollType::ROLL);
}

void AOClient::cmdRollP(int argc, QStringList argv)
{
    diceThrower(argc, argv, RollType::ROLLP);
}

void AOClient::cmdDoc(int argc, QStringList argv)
{
    QString sender_name = ooc_name;
    AreaData* area = server->areas[current_area];
    if (argc == 0) {
        sendServerMessage("Document: " + area->document);
    }
    else {
        area->document = argv.join(" ");
        sendServerMessageArea(sender_name + " changed the document.");
    }
}

void AOClient::cmdClearDoc(int argc, QStringList argv)
{
    QString sender_name = ooc_name;
    AreaData* area = server->areas[current_area];
    area->document = "No document.";
    sendServerMessageArea(sender_name + " cleared the document.");
}

void AOClient::cmdCM(int argc, QStringList argv)
{
    QString sender_name = ooc_name;
    AreaData* area = server->areas[current_area];
    if (area->is_protected) {
        sendServerMessage("This area is protected, you may not become CM.");
        return;
    }
    else if (area->owners.isEmpty()) { // no one owns this area, and it's not protected
        area->owners.append(id);
        area->invited.append(id);
        sendServerMessageArea(sender_name + " is now CM in this area.");
        arup(ARUPType::CM, true);
    }
    else if (!area->owners.contains(id)) { // there is already a CM, and it isn't us
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
        area->owners.append(owner_candidate->id);
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
    int removed = area->owners.removeAll(id);
    area->invited.removeAll(id);
    sendServerMessage("You are no longer CM in this area.");
    arup(ARUPType::CM, true);
    if (area->owners.isEmpty())
        area->invited.clear();
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
    else if (area->invited.contains(invited_id)) {
        sendServerMessage("That ID is already on the invite list.");
        return;
    }
    area->invited.append(invited_id);
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
    else if (area->owners.contains(uninvited_id)) {
        sendServerMessage("You cannot uninvite a CM!");
        return;
    }
    else if (!area->invited.contains(uninvited_id)) {
        sendServerMessage("That ID is not on the invite list.");
        return;
    }
    area->invited.removeAll(uninvited_id);
    sendServerMessage("You uninvited ID " + argv[0]);
}

void AOClient::cmdLock(int argc, QStringList argv)
{
    AreaData* area = server->areas[current_area];
    if (area->locked == AreaData::LockStatus::LOCKED) {
        sendServerMessage("This area is already locked.");
        return;
    }
    sendServerMessageArea("This area is now locked.");
    area->locked = AreaData::LockStatus::LOCKED;
    for (AOClient* client : server->clients) {
        if (client->current_area == current_area && client->joined) {
            area->invited.append(client->id);
        }
    }
    arup(ARUPType::LOCKED, true);
}

void AOClient::cmdSpectatable(int argc, QStringList argv)
{
    AreaData* area = server->areas[current_area];
    if (area->locked == AreaData::LockStatus::SPECTATABLE) {
        sendServerMessage("This area is already in spectate mode.");
        return;
    }
    sendServerMessageArea("This area is now spectatable.");
    area->locked = AreaData::LockStatus::SPECTATABLE;
    for (AOClient* client : server->clients) {
        if (client->current_area == current_area && client->joined) {
            area->invited.append(client->id);
        }
    }
    arup(ARUPType::LOCKED, true);
}

void AOClient::cmdUnLock(int argc, QStringList argv)
{
    AreaData* area = server->areas[current_area];
    if (area->locked == AreaData::LockStatus::FREE) {
        sendServerMessage("This area is not locked.");
        return;
    }
    sendServerMessageArea("This area is now unlocked.");
    area->locked = AreaData::LockStatus::FREE;
    arup(ARUPType::LOCKED, true);
}

void AOClient::cmdTimer(int argc, QStringList argv)
{
    AreaData* area = server->areas[current_area];

    if (argc == 0) {
        QStringList timers;
        timers.append("Currently active timers:");
        QTimer* global_timer = server->timer;
        if (global_timer->isActive()) {
            QTime current_time = QTime(0,0).addMSecs(global_timer->remainingTime());
            timers.append("Global timer is at " + current_time.toString("hh:mm:ss.zzz"));
        }
        for (QTimer* timer : area->timers) {
            timers.append(getAreaTimer(area->index, timer));
        }
        sendServerMessage(timers.join("\n"));
        return;
    }
    bool ok;
    int timer_id = argv[0].toInt(&ok);
    if (!ok || timer_id < 0 || timer_id > 4) {
        sendServerMessage("Invalid timer ID. Timer ID must be a whole number between 0 and 4.");
        return;
    }

    if (argc == 1) {
        if (timer_id == 0) {
            QTimer* global_timer = server->timer;
            if (global_timer->isActive()) {
                QTime current_time = QTime(0, 0, 0, global_timer->remainingTime());
                sendServerMessage("Global timer is at " + current_time.toString("hh:mm:ss.zzz"));
                return;
            }
        }
        else {
            QTimer* timer = area->timers[timer_id - 1];
            sendServerMessage(getAreaTimer(area->index, timer));
            return;
        }
    }

    QTimer* requested_timer;
    if (timer_id == 0) {
        if (!checkAuth(ACLFlags.value("GLOBAL_TIMER"))) {
            sendServerMessage("You are not authorized to alter the global timer.");
            return;
        }
        requested_timer = server->timer;
    }
    else
        requested_timer = area->timers[timer_id - 1];
    QTime requested_time = QTime::fromString(argv[1], "hh:mm:ss");
    if (requested_time.isValid()) {
        requested_timer->setInterval(QTime(0,0).msecsTo(requested_time));
        requested_timer->start();
        sendServerMessage("Set timer " + QString::number(timer_id) + " to " + argv[1] + ".");
        sendPacket("TI", {QString::number(timer_id), QString::number(2)});
        sendPacket("TI", {QString::number(timer_id), QString::number(0), QString::number(QTime(0,0).msecsTo(requested_time))});
        return;
    }
    else {
        if (argv[1] == "start") {
            requested_timer->start();
            sendServerMessage("Started timer " + QString::number(timer_id) + ".");
            sendPacket("TI", {QString::number(timer_id), QString::number(2)});
            sendPacket("TI", {QString::number(timer_id), QString::number(0), QString::number(QTime(0,0).msecsTo(QTime(0,0).addMSecs(requested_timer->remainingTime())))});
        }
        else if (argv[1] == "pause" || argv[1] == "stop") {
            requested_timer->setInterval(requested_timer->remainingTime());
            requested_timer->stop();
            sendServerMessage("Stopped timer " + QString::number(timer_id) + ".");
            sendPacket("TI", {QString::number(timer_id), QString::number(1), QString::number(QTime(0,0).msecsTo(QTime(0,0).addMSecs(requested_timer->interval())))});
        }
        else if (argv[1] == "hide" || argv[1] == "unset") {
            requested_timer->setInterval(0);
            requested_timer->stop();
            sendServerMessage("Hid timer " + QString::number(timer_id) + ".");
            sendPacket("TI", {QString::number(timer_id), QString::number(3)});
        }
    }
}

QStringList AOClient::buildAreaList(int area_idx)
{
    QStringList entries;
    QString area_name = server->area_names[area_idx];
    AreaData* area = server->areas[area_idx];
    entries.append("=== " + area_name + " ===");
    switch (area->locked) {
        case AreaData::LockStatus::LOCKED:
            entries.append("[LOCKED]");
            break;
        case AreaData::LockStatus::SPECTATABLE:
            entries.append("[SPECTATABLE]");
            break;
        case AreaData::LockStatus::FREE:
        default:
            break;
    }
    entries.append("[" + QString::number(area->player_count) + " users][" + area->status + "]");
    for (AOClient* client : server->clients) {
        if (client->current_area == area_idx && client->joined) {
            QString char_entry = "[" + QString::number(client->id) + "] " + client->current_char;
            if (client->current_char == "")
                char_entry += "Spectator";
            if (area->owners.contains(client->id))
                char_entry.insert(0, "[CM] ");
            if (authenticated)
                char_entry += " (" + client->getIpid() + "): " + client->ooc_name;
            entries.append(char_entry);
        }
    }
    return entries;
}

int AOClient::genRand(int min, int max)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
    qsrand(QDateTime::currentMSecsSinceEpoch());
    quint32 random_number = (qrand() % (max - min + 1)) + min;
    return random_number;

#else
    quint32 random_number = QRandomGenerator::system()->bounded(min, max + 1);
    return random_number;
#endif
}

void AOClient::diceThrower(int argc, QStringList argv, RollType type)
{
    QString sender_name = ooc_name;
    int max_value = server->getDiceValue("max_value");
    int max_dice = server->getDiceValue("max_dice");
    int bounded_value;
    int bounded_amount;
    QString dice_results;

    if (argc == 0) {
        dice_results = QString::number(genRand(1, 6)); // Self-explanatory
    }
    else if (argc == 1) {
        bounded_value = qBound(1, argv[0].toInt(), max_value); // faces, max faces
        dice_results = QString::number(genRand(1, bounded_value));
    }
    else if (argc == 2) {
        bounded_value = qBound(1, argv[0].toInt(), max_value); // 1, faces, max faces
        bounded_amount = qBound(1, argv[1].toInt(), max_dice); // 1, amount, max amount

        for (int i = 1; i <= bounded_amount ; i++) // Loop as multiple dices are thrown
        {
            QString dice_result = QString::number(genRand(1, bounded_value));
            if (i == bounded_amount) {
                dice_results = dice_results.append(dice_result);
            }
            else {
                dice_results = dice_results.append(dice_result + ",");
            }
        }
    }
    // Switch to change message behaviour, isEmpty check or the entire server crashes due to an out of range issue in the QStringList
    switch(type)
    {
        case ROLL:
        if (argv.isEmpty()) {
            sendServerMessageArea(sender_name + " rolled " + dice_results + " out of 6");
        }
        else {
            sendServerMessageArea(sender_name + " rolled " + dice_results + " out of " + QString::number(bounded_value));
        }
        break;
        case ROLLP:
        if (argv.isEmpty()) {
            sendServerMessage(sender_name + " rolled " + dice_results + " out of 6");
            sendServerMessageArea((sender_name + " rolled in secret."));
        }
        else {
            sendServerMessageArea(sender_name + " rolled " + dice_results + " out of " + QString::number(bounded_value));
            sendServerMessageArea((sender_name + " rolled in secret."));
        }
        break;
        case ROLLA:
        //Not implemented yet
        default : break;
    }
}

QString AOClient::getAreaTimer(int area_idx, QTimer* timer)
{
    AreaData* area = server->areas[area_idx];
    if (timer->isActive()) {
        QTime current_time = QTime(0,0).addMSecs(timer->remainingTime());
        return "Timer " + QString::number(area->timers.indexOf(timer) + 1) + " is at " + current_time.toString("hh:mm:ss.zzz");
    }
    else {
        return "Timer " + QString::number(area->timers.indexOf(timer) + 1) + " is inactive.";
    }
}

