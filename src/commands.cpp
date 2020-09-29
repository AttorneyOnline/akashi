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

    // TODO: tell the user if no modpass is set
    if (auth_type == "simple") {
        if(argv[0] == modpass) {
            sendServerMessage("Logged in as a moderator."); // This string has to be exactly this, because it is hardcoded in the client
            authenticated = true;
        } else {
            sendServerMessage("Incorrect password.");
            return;
        }
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
            sendServerMessage("Logged in as " + username);
        }
        else {
            sendServerMessage("Incorrect password.");
        }
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
        sendServerMessage("Banned user with ipid " + target_ipid + " for reason: " + reason);
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

    quint64 salt_number = QRandomGenerator::system()->generate64();
    QString salt = QStringLiteral("%1").arg(salt_number, 16, 16, QLatin1Char('0'));

    server->db_manager->createUser("root", salt, argv[0], ACLFlags::SUPER);
}

QStringList AOClient::buildAreaList(int area_idx)
{
    QStringList entries;
    QString area_name = server->area_names[area_idx];
    AreaData* area = server->areas[area_idx];
    entries.append("=== " + area_name + " ===");
    entries.append("[" + QString::number(area->player_count) + " users][" + area->status + "]");
    for (AOClient* client : server->clients) {
        if (client->current_area == area_idx) {
            QString char_entry = client->current_char;
            if (authenticated)
                char_entry += " (" + client->getIpid() + "): " + ooc_name;
            entries.append(char_entry);
        }
    }
    return entries;
}
