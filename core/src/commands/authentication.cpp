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

#include "include/config_manager.h"
#include "include/db_manager.h"
#include "include/server.h"

// This file is for commands under the authentication category in aoclient.h
// Be sure to register the command in the header before adding it here!

void AOClient::cmdLogin(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    if (m_authenticated) {
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
            m_is_logging_in = true;
            return;
        }
        break;
    case DataTypes::AuthType::ADVANCED:
        sendServerMessage("Entering login prompt.\nPlease enter your username and password.");
        m_is_logging_in = true;
        return;
        break;
    }
}

void AOClient::cmdChangeAuth(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    if (ConfigManager::authType() == DataTypes::AuthType::SIMPLE) {
        change_auth_started = true;
        sendServerMessage("WARNING!\nThis command will change how logging in as a moderator works.\nOnly proceed if you know what you are doing\nUse the command /rootpass to set the password for your root account.");
    }
}

void AOClient::cmdSetRootPass(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    if (!change_auth_started)
        return;

    if (!checkPasswordRequirements("root", argv[0])) {
        sendServerMessage("Password does not meet server requirements.");
        return;
    }

    sendServerMessage("Changing auth type and setting root password.\nLogin again with /login root [password]");
    m_authenticated = false;
    ConfigManager::setAuthType(DataTypes::AuthType::ADVANCED);

#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
    qsrand(QDateTime::currentMSecsSinceEpoch());
    quint32 l_upper_salt = qrand();
    quint32 l_lower_salt = qrand();
    quint64 l_salt_number = (upper_salt << 32) | lower_salt;
#else
    quint64 l_salt_number = QRandomGenerator::system()->generate64();
#endif
    QString l_salt = QStringLiteral("%1").arg(l_salt_number, 16, 16, QLatin1Char('0'));

    server->getDatabaseManager()->createUser("root", l_salt, argv[0], ACLFlags.value("SUPER"));
}

void AOClient::cmdAddUser(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    if (!checkPasswordRequirements(argv[0], argv[1])) {
        sendServerMessage("Password does not meet server requirements.");
        return;
    }
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
    qsrand(QDateTime::currentMSecsSinceEpoch());
    quint32 l_upper_salt = qrand();
    quint32 l_lower_salt = qrand();
    quint64 l_salt_number = (upper_salt << 32) | lower_salt;
#else
    quint64 l_salt_number = QRandomGenerator::system()->generate64();
#endif
    QString l_salt = QStringLiteral("%1").arg(l_salt_number, 16, 16, QLatin1Char('0'));

    if (server->getDatabaseManager()->createUser(argv[0], l_salt, argv[1], ACLFlags.value("NONE")))
        sendServerMessage("Created user " + argv[0] + ".\nUse /addperm to modify their permissions.");
    else
        sendServerMessage("Unable to create user " + argv[0] + ".\nDoes a user with that name already exist?");
}

void AOClient::cmdRemoveUser(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    if (server->getDatabaseManager()->deleteUser(argv[0]))
        sendServerMessage("Successfully removed user " + argv[0] + ".");
    else
        sendServerMessage("Unable to remove user " + argv[0] + ".\nDoes it exist?");
}

void AOClient::cmdListPerms(int argc, QStringList argv)
{
    unsigned long long l_user_acl = server->getDatabaseManager()->getACL(m_moderator_name);
    QStringList l_message;
    const QStringList l_keys = ACLFlags.keys();
    if (argc == 0) {
        // Just print out all permissions available to the user.
        l_message.append("You have been given the following permissions:");
        for (const QString &l_perm : l_keys) {
            if (l_perm == "NONE")
                ; // don't need to list this one
            else if (l_perm == "SUPER") {
                if (l_user_acl == ACLFlags.value("SUPER")) // This has to be checked separately, because SUPER & anything will always be truthy
                    l_message.append("SUPER (Be careful! This grants the user all permissions.)");
            }
            else if ((ACLFlags.value(l_perm) & l_user_acl) == 0)
                ; // user doesn't have this permission, don't print it
            else
                l_message.append(l_perm);
        }
    }
    else {
        if ((l_user_acl & ACLFlags.value("MODIFY_USERS")) == 0) {
            sendServerMessage("You do not have permission to view other users' permissions.");
            return;
        }

        l_message.append("User " + argv[0] + " has the following permissions:");
        unsigned long long l_acl = server->getDatabaseManager()->getACL(argv[0]);
        if (l_acl == 0) {
            sendServerMessage("This user either doesn't exist, or has no permissions set.");
            return;
        }

        for (const QString &l_perm : l_keys) {
            if ((ACLFlags.value(l_perm) & l_acl) != 0 && l_perm != "SUPER") {
                l_message.append(l_perm);
            }
        }
    }
    sendServerMessage(l_message.join("\n"));
}

void AOClient::cmdAddPerms(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    unsigned long long l_user_acl = server->getDatabaseManager()->getACL(m_moderator_name);
    argv[1] = argv[1].toUpper();
    const QStringList l_keys = ACLFlags.keys();

    if (!l_keys.contains(argv[1])) {
        sendServerMessage("That permission doesn't exist!");
        return;
    }

    if (argv[1] == "SUPER") {
        if (l_user_acl != ACLFlags.value("SUPER")) {
            // This has to be checked separately, because SUPER & anything will always be truthy
            sendServerMessage("You aren't allowed to add that permission!");
            return;
        }
    }
    if (argv[1] == "NONE") {
        sendServerMessage("Added no permissions!");
        return;
    }

    unsigned long long l_newperm = ACLFlags.value(argv[1]);
    if ((l_newperm & l_user_acl) != 0) {
        if (server->getDatabaseManager()->updateACL(argv[0], l_newperm, true))
            sendServerMessage("Successfully added permission " + argv[1] + " to user " + argv[0]);
        else
            sendServerMessage(argv[0] + " wasn't found!");
        return;
    }

    sendServerMessage("You aren't allowed to add that permission!");
}

void AOClient::cmdRemovePerms(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    unsigned long long l_user_acl = server->getDatabaseManager()->getACL(m_moderator_name);
    argv[1] = argv[1].toUpper();

    const QStringList l_keys = ACLFlags.keys();

    if (!l_keys.contains(argv[1])) {
        sendServerMessage("That permission doesn't exist!");
        return;
    }

    if (argv[0] == "root") {
        sendServerMessage("You cannot change the permissions of the root account!");
        return;
    }

    if (argv[1] == "SUPER") {
        if (l_user_acl != ACLFlags.value("SUPER")) {
            // This has to be checked separately, because SUPER & anything will always be truthy
            sendServerMessage("You aren't allowed to remove that permission!");
            return;
        }
    }
    if (argv[1] == "NONE") {
        sendServerMessage("Removed no permissions!");
        return;
    }

    unsigned long long l_newperm = ACLFlags.value(argv[1]);
    if ((l_newperm & l_user_acl) != 0) {
        if (server->getDatabaseManager()->updateACL(argv[0], l_newperm, false))
            sendServerMessage("Successfully removed permission " + argv[1] + " from user " + argv[0]);
        else
            sendServerMessage(argv[0] + " wasn't found!");
        return;
    }

    sendServerMessage("You aren't allowed to remove that permission!");
}

void AOClient::cmdListUsers(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QStringList l_users = server->getDatabaseManager()->getUsers();
    sendServerMessage("All users:\n" + l_users.join("\n"));
}

void AOClient::cmdLogout(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    if (!m_authenticated) {
        sendServerMessage("You are not logged in!");
        return;
    }
    m_authenticated = false;
    m_moderator_name = "";
    sendPacket("AUTH", {"-1"}); // Client: "You were logged out."
}

void AOClient::cmdChangePassword(int argc, QStringList argv)
{
    QString l_username;
    QString l_password = argv[0];
    if (argc == 1) {
        if (m_moderator_name.isEmpty()) {
            sendServerMessage("You are not logged in.");
            return;
        }
        l_username = m_moderator_name;
    }
    else if (argc == 2 && checkAuth(ACLFlags.value("SUPER"))) {
        l_username = argv[1];
    }
    else {
        sendServerMessage("Invalid command syntax.");
        return;
    }

    if (!checkPasswordRequirements(l_username, l_password)) {
        sendServerMessage("Password does not meet server requirements.");
        return;
    }

    if (server->getDatabaseManager()->updatePassword(l_username, l_password)) {
        sendServerMessage("Successfully changed password.");
    }
    else {
        sendServerMessage("There was an error changing the password.");
        return;
    }
}
