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

// This file is for commands under the moderation category in aoclient.h
// Be sure to register the command in the header before adding it here!

void AOClient::cmdBan(int argc, QStringList argv)
{
    QString args_str = argv[2];
    if (argc > 3) {
        for (int i = 3; i < argc; i++)
            args_str += " " + argv[i];
    }

    DBManager::BanInfo ban;

    long long duration_seconds = 0;
    if (argv[1] == "perma")
        duration_seconds = -2;
    else
        duration_seconds = parseTime(argv[1]);

    if (duration_seconds == -1) {
        sendServerMessage("Invalid time format. Format example: 1h30m");
        return;
    }

    ban.duration = duration_seconds;
    ban.ipid = argv[0];
    ban.reason = args_str;
    ban.time = QDateTime::currentDateTime().toSecsSinceEpoch();
    bool ban_logged = false;
    int kick_counter = 0;

    switch (ConfigManager::authType()) {
    case DataTypes::AuthType::SIMPLE:
        ban.moderator = "moderator";
        break;
    case DataTypes::AuthType::ADVANCED:
        ban.moderator = moderator_name;
        break;
    }

    for (AOClient* client : server->getClientsByIpid(ban.ipid)) {
        if (!ban_logged) {
            ban.ip = client->remote_ip;
            ban.hdid = client->hwid;
            server->db_manager->addBan(ban);
            sendServerMessage("Banned user with ipid " + ban.ipid + " for reason: " + ban.reason);
            ban_logged = true;
        }
        QString ban_duration;
        if (!(ban.duration == -2)) {
            ban_duration = QDateTime::fromSecsSinceEpoch(ban.time).addSecs(ban.duration).toString("MM/dd/yyyy, hh:mm");
        }
        else {
            ban_duration = "The heat death of the universe.";
        }
        int ban_id = server->db_manager->getBanID(ban.ip);
        client->sendPacket("KB", {ban.reason + "\nID: " + QString::number(ban_id) + "\nUntil: " + ban_duration});
        client->socket->close();
        kick_counter++;

        if (ConfigManager::discordBanWebhookEnabled())
            emit server->banWebhookRequest(ban.ipid, ban.moderator, ban_duration, ban.reason, ban_id);
    }

    if (kick_counter > 1)
        sendServerMessage("Kicked " + QString::number(kick_counter) + " clients with matching ipids.");

    // We're banning someone not connected.
    if (!ban_logged) {
        server->db_manager->addBan(ban);
        sendServerMessage("Banned " + ban.ipid + " for reason: " + ban.reason);
    }
}

void AOClient::cmdKick(int argc, QStringList argv)
{
    QString target_ipid = argv[0];
    QString reason = argv[1];
    int kick_counter = 0;

    if (argc > 2) {
        for (int i = 2; i < argv.length(); i++) {
            reason += " " + argv[i];
        }
    }

    for (AOClient* client : server->getClientsByIpid(target_ipid)) {
        client->sendPacket("KK", {reason});
        client->socket->close();
        kick_counter++;
    }

    if (kick_counter > 0)
        sendServerMessage("Kicked " + QString::number(kick_counter) + " client(s) with ipid " + target_ipid + " for reason: " + reason);
    else
        sendServerMessage("User with ipid not found!");
}

void AOClient::cmdMods(int argc, QStringList argv)
{
    QStringList entries;
    int online_count = 0;
    for (AOClient* client : server->clients) {
        if (client->authenticated) {
            entries << "---";
            if (ConfigManager::authType() != DataTypes::AuthType::SIMPLE)
                entries << "Moderator: " + client->moderator_name;
            entries << "OOC name: " + client->ooc_name;
            entries << "ID: " + QString::number(client->id);
            entries << "Area: " + QString::number(client->current_area);
            entries << "Character: " + client->current_char;
            online_count++;
        }
    }
    entries << "---";
    entries << "Total online: " << QString::number(online_count);
    sendServerMessage(entries.join("\n"));
}

void AOClient::cmdHelp(int argc, QStringList argv)
{
    QStringList entries;
    entries << "Allowed commands:";
    QMap<QString, CommandInfo>::const_iterator i;
    for (i = commands.constBegin(); i!= commands.constEnd(); ++i) {
        CommandInfo info = i.value();
        if (checkAuth(info.acl_mask)) { // if we are allowed to use this command
            entries << "/" + i.key();
        }
    }
    sendServerMessage(entries.join("\n"));
}

void AOClient::cmdMOTD(int argc, QStringList argv)
{
    if (argc == 0) {
        sendServerMessage("=== MOTD ===\r\n" + ConfigManager::motd() + "\r\n=============");
    }
    else if (argc > 0) {
        if (checkAuth(ACLFlags.value("MOTD"))) {
            QString MOTD = argv.join(" ");
            ConfigManager::setMotd(MOTD);
            sendServerMessage("MOTD has been changed.");
        }
        else {
            sendServerMessage("You do not have permission to change the MOTD");
        }
    }
}

void AOClient::cmdBans(int argc, QStringList argv)
{
    QStringList recent_bans;
    recent_bans << "Last 5 bans:";
    recent_bans << "-----";
    for (DBManager::BanInfo ban : server->db_manager->getRecentBans()) {
        QString banned_until;
        if (ban.duration == -2)
            banned_until = "The heat death of the universe";
        else
            banned_until = QDateTime::fromSecsSinceEpoch(ban.time).addSecs(ban.duration).toString("MM/dd/yyyy, hh:mm");
        recent_bans << "Ban ID: " + QString::number(ban.id);
        recent_bans << "Affected IPID: " + ban.ipid;
        recent_bans << "Affected HDID: " + ban.hdid;
        recent_bans << "Reason for ban: " + ban.reason;
        recent_bans << "Date of ban: " + QDateTime::fromSecsSinceEpoch(ban.time).toString("MM/dd/yyyy, hh:mm");
        recent_bans << "Ban lasts until: " + banned_until;
        recent_bans << "Moderator: " + ban.moderator;
        recent_bans << "-----";
    }
    sendServerMessage(recent_bans.join("\n"));
}

void AOClient::cmdUnBan(int argc, QStringList argv)
{
    bool ok;
    int target_ban = argv[0].toInt(&ok);
    if (!ok) {
        sendServerMessage("Invalid ban ID.");
        return;
    }
    else if (server->db_manager->invalidateBan(target_ban))
        sendServerMessage("Successfully invalidated ban " + argv[0] + ".");
    else
        sendServerMessage("Couldn't invalidate ban " + argv[0] + ", are you sure it exists?");
}

void AOClient::cmdAbout(int argc, QStringList argv)
{
    sendPacket("CT", {"The akashi dev team", "Thank you for using akashi! Made with love by scatterflower, with help from in1tiate, Salanto, and mangosarentliterature. akashi " + QCoreApplication::applicationVersion() + ". For documentation and reporting issues, see the source: https://github.com/AttorneyOnline/akashi"});
}

void AOClient::cmdMute(int argc, QStringList argv)
{
    bool conv_ok = false;
    int uid = argv[0].toInt(&conv_ok);
    if (!conv_ok) {
        sendServerMessage("Invalid user ID.");
        return;
    }

    AOClient* target = server->getClientByID(uid);

    if (target == nullptr) {
        sendServerMessage("No client with that ID found.");
        return;
    }

    if (target->is_muted)
        sendServerMessage("That player is already muted!");
    else {
        sendServerMessage("Muted player.");
        target->sendServerMessage("You were muted by a moderator. " + getReprimand());
    }
    target->is_muted = true;
}

void AOClient::cmdUnMute(int argc, QStringList argv)
{
    bool conv_ok = false;
    int uid = argv[0].toInt(&conv_ok);
    if (!conv_ok) {
        sendServerMessage("Invalid user ID.");
        return;
    }

    AOClient* target = server->getClientByID(uid);

    if (target == nullptr) {
        sendServerMessage("No client with that ID found.");
        return;
    }

    if (!target->is_muted)
        sendServerMessage("That player is not muted!");
    else {
        sendServerMessage("Unmuted player.");
        target->sendServerMessage("You were unmuted by a moderator. " + getReprimand(true));
    }
    target->is_muted = false;
}

void AOClient::cmdOocMute(int argc, QStringList argv)
{
    bool conv_ok = false;
    int uid = argv[0].toInt(&conv_ok);
    if (!conv_ok) {
        sendServerMessage("Invalid user ID.");
        return;
    }

    AOClient* target = server->getClientByID(uid);

    if (target == nullptr) {
        sendServerMessage("No client with that ID found.");
        return;
    }

    if (target->is_ooc_muted)
        sendServerMessage("That player is already OOC muted!");
    else {
        sendServerMessage("OOC muted player.");
        target->sendServerMessage("You were OOC muted by a moderator. " + getReprimand());
    }
    target->is_ooc_muted = true;
}

void AOClient::cmdOocUnMute(int argc, QStringList argv)
{
    bool conv_ok = false;
    int uid = argv[0].toInt(&conv_ok);
    if (!conv_ok) {
        sendServerMessage("Invalid user ID.");
        return;
    }

    AOClient* target = server->getClientByID(uid);

    if (target == nullptr) {
        sendServerMessage("No client with that ID found.");
        return;
    }

    if (!target->is_ooc_muted)
        sendServerMessage("That player is not OOC muted!");
    else {
        sendServerMessage("OOC unmuted player.");
        target->sendServerMessage("You were OOC unmuted by a moderator. " + getReprimand(true));
    }
    target->is_ooc_muted = false;
}

void AOClient::cmdBlockWtce(int argc, QStringList argv)
{
    bool conv_ok = false;
    int uid = argv[0].toInt(&conv_ok);
    if (!conv_ok) {
        sendServerMessage("Invalid user ID.");
        return;
    }

    AOClient* target = server->getClientByID(uid);

    if (target == nullptr) {
        sendServerMessage("No client with that ID found.");
        return;
    }

    if (target->is_wtce_blocked)
        sendServerMessage("That player is already judge blocked!");
    else {
        sendServerMessage("Revoked player's access to judge controls.");
        target->sendServerMessage("A moderator revoked your judge controls access. " + getReprimand());
    }
    target->is_wtce_blocked = true;
}

void AOClient::cmdUnBlockWtce(int argc, QStringList argv)
{
    bool conv_ok = false;
    int uid = argv[0].toInt(&conv_ok);
    if (!conv_ok) {
        sendServerMessage("Invalid user ID.");
        return;
    }

    AOClient* target = server->getClientByID(uid);

    if (target == nullptr) {
        sendServerMessage("No client with that ID found.");
        return;
    }

    if (!target->is_wtce_blocked)
        sendServerMessage("That player is not judge blocked!");
    else {
        sendServerMessage("Restored player's access to judge controls.");
        target->sendServerMessage("A moderator restored your judge controls access. " + getReprimand(true));
    }
    target->is_wtce_blocked = false;
}

void AOClient::cmdAllowBlankposting(int argc, QStringList argv)
{
    QString sender_name = ooc_name;
    AreaData* area = server->areas[current_area];
    area->toggleBlankposting();
    if (area->blankpostingAllowed() == false) {
        sendServerMessageArea(sender_name + " has set blankposting in the area to forbidden.");
    }
    else {
        sendServerMessageArea(sender_name + " has set blankposting in the area to allowed.");
    }
}

void AOClient::cmdBanInfo(int argc, QStringList argv)
{
    QStringList ban_info;
    ban_info << ("Ban Info for " + argv[0]);
    ban_info << "-----";
    QString lookup_type;

    if (argc == 1) {
       lookup_type = "banid";
    }
    else if (argc == 2) {
        lookup_type = argv[1];
        if (!((lookup_type == "banid") || (lookup_type == "ipid") || (lookup_type == "hdid"))) {
            sendServerMessage("Invalid ID type.");
            return;
        }
    }
    else {
        sendServerMessage("Invalid command.");
        return;
    }
    QString id = argv[0];
    for (DBManager::BanInfo ban : server->db_manager->getBanInfo(lookup_type, id)) {
        QString banned_until;
        if (ban.duration == -2)
            banned_until = "The heat death of the universe";
        else
            banned_until = QDateTime::fromSecsSinceEpoch(ban.time).addSecs(ban.duration).toString("MM/dd/yyyy, hh:mm");
        ban_info << "Ban ID: " + QString::number(ban.id);
        ban_info << "Affected IPID: " + ban.ipid;
        ban_info << "Affected HDID: " + ban.hdid;
        ban_info << "Reason for ban: " + ban.reason;
        ban_info << "Date of ban: " + QDateTime::fromSecsSinceEpoch(ban.time).toString("MM/dd/yyyy, hh:mm");
        ban_info << "Ban lasts until: " + banned_until;
        ban_info << "Moderator: " + ban.moderator;
        ban_info << "-----";
    }
    sendServerMessage(ban_info.join("\n"));
}

void AOClient::cmdReload(int argc, QStringList argv)
{
    ConfigManager::reloadSettings();
    emit server->reloadRequest(ConfigManager::serverName(), ConfigManager::serverDescription());
    server->updateHTTPAdvertiserConfig();
    sendServerMessage("Reloaded configurations");
}

void AOClient::cmdForceImmediate(int argc, QStringList argv)
{
    AreaData* area = server->areas[current_area];
    area->toggleImmediate();
    QString state = area->forceImmediate() ? "on." : "off.";
    sendServerMessage("Forced immediate text processing in this area is now " + state);
}

void AOClient::cmdAllowIniswap(int argc, QStringList argv)
{
    AreaData* area = server->areas[current_area];
    area->toggleIniswap();
    QString state = area->iniswapAllowed() ? "allowed." : "disallowed.";
    sendServerMessage("Iniswapping in this area is now " + state);
}

void AOClient::cmdPermitSaving(int argc, QStringList argv)
{
    AOClient* client = server->getClientByID(argv[0].toInt());
    if (client == nullptr) {
        sendServerMessage("Invalid ID.");
        return;
    }
    client->testimony_saving = true;
}

void AOClient::cmdKickUid(int argc, QStringList argv)
{
    QString reason = argv[1];

    if (argc > 2) {
        for (int i = 2; i < argv.length(); i++) {
            reason += " " + argv[i];
        }
    }

    bool conv_ok = false;
    int uid = argv[0].toInt(&conv_ok);
    if (!conv_ok) {
        sendServerMessage("Invalid user ID.");
        return;
    }

    AOClient* target = server->getClientByID(uid);
    if (target == nullptr) {
        sendServerMessage("No client with that ID found.");
        return;
    }
    target->sendPacket("KK", {reason});
    target->socket->close();
    sendServerMessage("Kicked client with UID " + argv[0] + " for reason: " + reason);
}

void AOClient::cmdUpdateBan(int argc, QStringList argv)
{
    bool conv_ok = false;
    int ban_id = argv[0].toInt(&conv_ok);
    if (!conv_ok) {
        sendServerMessage("Invalid ban ID.");
        return;
    }
    QVariant updated_info;
    if (argv[1] == "duration") {
        long long duration_seconds = 0;
        if (argv[2] == "perma")
            duration_seconds = -2;
        else
            duration_seconds = parseTime(argv[2]);

        if (duration_seconds == -1) {
            sendServerMessage("Invalid time format. Format example: 1h30m");
            return;
        }
        updated_info = QVariant(duration_seconds);

    }
    else if (argv[1] == "reason") {
        QString args_str = argv[2];
        if (argc > 3) {
            for (int i = 3; i < argc; i++)
                args_str += " " + argv[i];
        }
        updated_info = QVariant(args_str);
    }
    else {
        sendServerMessage("Invalid update type.");
        return;
    }
    if (!server->db_manager->updateBan(ban_id, argv[1], updated_info)) {
        sendServerMessage("There was an error updating the ban. Please confirm the ban ID is valid.");
        return;
    }
    sendServerMessage("Ban updated.");
}
