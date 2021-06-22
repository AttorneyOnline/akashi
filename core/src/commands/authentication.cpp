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

// This file is for commands under the authentication category in aoclient.h
// Be sure to register the command in the header before adding it here!

void AOClient::cmdLogin(int argc, QStringList argv)
{
    if (authenticated) {
        sendServerMessage("You are already logged in!");
        return;
    }
    switch (ConfigManager::authType()) {
    case DataTypes::AuthType::SIMPLE:
        if (ConfigManager::modpass() == "") {
            sendServerMessage("No modpass is set. Please set a modpass before logging in.");
            return;
        }
        else {
            sendServerMessage("Entering login prompt.\nPlease enter the server modpass.");
            is_logging_in = true;
            return;
        }
        break;
    case DataTypes::AuthType::ADVANCED:
        sendServerMessage("Entering login prompt.\nPlease enter your username and password.");
        is_logging_in = true;
        return;
        break;
    }
}

void AOClient::cmdChangeAuth(int argc, QStringList argv)
{
    if (ConfigManager::authType() == DataTypes::AuthType::SIMPLE) {
        change_auth_started = true;
        sendServerMessage("WARNING!\nThis command will change how logging in as a moderator works.\nOnly proceed if you know what you are doing\nUse the command /rootpass to set the password for your root account.");
    }
}

void AOClient::cmdSetRootPass(int argc, QStringList argv)
{
    if (!change_auth_started)
        return;

    if (!checkPasswordRequirements("root", argv[0])) {
        sendServerMessage("Password does not meet server requirements.");
        return;
    }

    sendServerMessage("Changing auth type and setting root password.\nLogin again with /login root [password]");
    authenticated = false;
    ConfigManager::setAuthType(DataTypes::AuthType::ADVANCED);

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

void AOClient::cmdAddUser(int argc, QStringList argv)
{
    if (!checkPasswordRequirements(argv[0], argv[1])) {
        sendServerMessage("Password does not meet server requirements.");
        return;
    }
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

void AOClient::cmdRemoveUser(int argc, QStringList argv)
{
    if (server->db_manager->deleteUser(argv[0]))
        sendServerMessage("Successfully removed user " + argv[0] + ".");
    else
        sendServerMessage("Unable to remove user " + argv[0] + ".\nDoes it exist?");
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

void AOClient::cmdChangePassword(int argc, QStringList argv)
{
    QString username;
    QString password;
    if (argc == 1) {
        if (moderator_name.isEmpty()) {
            sendServerMessage("You are not logged in.");
            return;
        }
        username = moderator_name;
        password = argv[0];
    }
    else if (argc == 2 && checkAuth(ACLFlags.value("SUPER"))) {
        username = argv[0];
        password = argv[1];
    }
    else {
        sendServerMessage("Invalid command syntax.");
        return;
    }

    if (!checkPasswordRequirements(username, password)) {
        sendServerMessage("Password does not meet server requirements.");
        return;
    }

    if (server->db_manager->updatePassword(username, password)) {
        sendServerMessage("Successfully changed password.");
    }
    else {
        sendServerMessage("There was an error changing the password.");
        return;
    }
}
