#include "commands/authentication_commands.h"

#include "acl_roles_handler.h"
#include "core/command_context.h"
#include "core/server_settings.h"
#include "core/command_registry.h"
#include "core/permission_registry.h"
#include "crypto_helper.h"
#include "db_manager.h"
#include "server.h"

#include <QRegularExpression>
#include <QSet>

namespace akashi::commands {

static QSet<int> s_change_auth_started;

static QString decodeMessage(const QString &f_message)
{
    return QString(f_message)
        .replace("<num>", "#")
        .replace("<percent>", "%")
        .replace("<dollar>", "$")
        .replace("<and>", "&");
}

static bool checkPasswordRequirements(ServerSettings *f_settings, const QString &f_username, const QString &f_password)
{
    QString l_decoded_password = decodeMessage(f_password);
    if (!f_settings->password_requirements())
        return true;

    if (f_settings->pass_min_length() > l_decoded_password.length())
        return false;

    if (f_settings->pass_max_length() < l_decoded_password.length() && f_settings->pass_max_length() != 0)
        return false;

    if (f_settings->pass_required_mix_case()) {
        if (l_decoded_password.toLower() == l_decoded_password)
            return false;
        if (l_decoded_password.toUpper() == l_decoded_password)
            return false;
    }

    if (f_settings->pass_required_numbers()) {
        QRegularExpression l_regex(QStringLiteral("[0123456789]"));
        QRegularExpressionMatch l_match = l_regex.match(l_decoded_password);
        if (!l_match.hasMatch())
            return false;
    }

    if (f_settings->pass_required_special()) {
        QRegularExpression l_regex(QStringLiteral("[~!@#$%^&*_\\-+=`|\\\\(){}\\[\\]:;\"'<>,.?/]"));
        QRegularExpressionMatch l_match = l_regex.match(l_decoded_password);
        if (!l_match.hasMatch())
            return false;
    }

    if (!f_settings->pass_can_contain_username()) {
        if (l_decoded_password.contains(f_username))
            return false;
    }

    return true;
}

static void handleLogin(CommandContext &f_context)
{
    if (f_context.isAuthenticated()) {
        f_context.reply("You are already logged in!");
        return;
    }

    switch (f_context.server()->authType()) {
    case DataTypes::AuthType::SIMPLE:
        if (f_context.server()->serverSettings()->modpass().isEmpty()) {
            f_context.reply("No modpass is set. Please set a modpass before logging in.");
            return;
        }
        f_context.reply("Entering login prompt.\nPlease enter the server modpass.");
        f_context.setInLoginPrompt(true);
        return;
    case DataTypes::AuthType::ADVANCED:
        f_context.reply("Entering login prompt.\nPlease enter your username and password.");
        f_context.setInLoginPrompt(true);
        return;
    }
}

static void handleChangeAuth(CommandContext &f_context)
{
    if (f_context.server()->authType() == DataTypes::AuthType::SIMPLE) {
        s_change_auth_started.insert(f_context.clientId());
        f_context.reply("WARNING!\nThis command will change how logging in as a moderator works.\n"
                        "Only proceed if you know what you are doing\n"
                        "Use the command /rootpass to set the password for your root account.");
    }
}

static void handleSetRootPass(CommandContext &f_context)
{
    if (!s_change_auth_started.contains(f_context.clientId()))
        return;

    s_change_auth_started.remove(f_context.clientId());

    if (!checkPasswordRequirements(f_context.server()->serverSettings(), QStringLiteral("root"), f_context.argument(0))) {
        f_context.reply("Password does not meet server requirements.");
        return;
    }

    f_context.reply("Changing auth type and setting root password.\nLogin again with /login root [password]");
    f_context.setAuthenticated(false);
    f_context.server()->setAuthType(DataTypes::AuthType::ADVANCED);

    QByteArray l_salt = CryptoHelper::randbytes(16);
    f_context.server()->databaseManager()->createUser(QStringLiteral("root"), l_salt, f_context.argument(0), ACLRolesHandler::SUPER_ID);
}

static void handleAddUser(CommandContext &f_context)
{
    if (!checkPasswordRequirements(f_context.server()->serverSettings(), f_context.argument(0), f_context.argument(1))) {
        f_context.reply("Password does not meet server requirements.");
        return;
    }

    QByteArray l_salt = CryptoHelper::randbytes(16);

    if (f_context.server()->databaseManager()->createUser(f_context.argument(0), l_salt, f_context.argument(1), ACLRolesHandler::NONE_ID))
        f_context.reply("Created user " + f_context.argument(0) + ".\nUse /setperms to modify their permissions.");
    else
        f_context.reply("Unable to create user " + f_context.argument(0) + ".\nDoes a user with that name already exist?");
}

static void handleRemoveUser(CommandContext &f_context)
{
    if (f_context.server()->databaseManager()->deleteUser(f_context.argument(0)))
        f_context.reply("Successfully removed user " + f_context.argument(0) + ".");
    else
        f_context.reply("Unable to remove user " + f_context.argument(0) + ".\nDoes it exist?");
}

static void handleListPerms(CommandContext &f_context)
{
    const ACLRole l_role = f_context.server()->aclRolesHandler()->roleById(f_context.aclRoleId());

    ACLRole l_target_role = l_role;
    QStringList l_message;

    if (f_context.argc() == 0) {
        l_message.append("You have been given the following permissions:");
    }
    else {
        if (!l_role.canPerform(ACLRole::MODIFY_USERS)) {
            f_context.reply("You do not have permission to view other users' permissions.");
            return;
        }
        l_message.append("User " + f_context.argument(0) + " has the following permissions:");
        l_target_role = f_context.server()->aclRolesHandler()->roleById(f_context.argument(0));
    }

    if (l_target_role.permissions() == ACLRole::NONE) {
        l_message.append("NONE");
    }
    else if (l_target_role.canPerform(ACLRole::SUPER)) {
        l_message.append("SUPER (Be careful! This grants the user all permissions.)");
    }
    else {
        const QList<ACLRole::Permission> l_permissions = ACLRole::PERMISSION_CAPTIONS.keys();
        for (const ACLRole::Permission i_permission : l_permissions) {
            if (l_target_role.canPerform(i_permission)) {
                l_message.append(ACLRole::PERMISSION_CAPTIONS.value(i_permission));
            }
        }
    }

    f_context.reply(l_message.join("\n"));
}

static void handleSetPerms(CommandContext &f_context)
{
    const QString l_target_acl = f_context.argument(1);
    if (!f_context.server()->aclRolesHandler()->roleExists(l_target_acl)) {
        f_context.reply("That role doesn't exist!");
        return;
    }

    if (l_target_acl == ACLRolesHandler::SUPER_ID && !f_context.canPerform(akashi::permission::super)) {
        f_context.reply("You aren't allowed to set that role!");
        return;
    }

    const QString l_target_username = f_context.argument(0);
    if (l_target_username == "root") {
        f_context.reply("You can't change root's role!");
        return;
    }

    if (f_context.server()->databaseManager()->updateACL(l_target_username, l_target_acl))
        f_context.reply("Successfully applied role " + l_target_acl + " to user " + l_target_username);
    else
        f_context.reply(l_target_username + " wasn't found!");
}

static void handleRemovePerms(CommandContext &f_context)
{
    const QString l_username = f_context.argument(0);
    if (l_username == "root") {
        f_context.reply("You can't change root's role!");
        return;
    }

    if (f_context.server()->databaseManager()->updateACL(l_username, ACLRolesHandler::NONE_ID))
        f_context.reply("Successfully applied role " + ACLRolesHandler::NONE_ID + " to user " + l_username);
    else
        f_context.reply(l_username + " wasn't found!");
}

static void handleListUsers(CommandContext &f_context)
{
    QStringList l_users = f_context.server()->databaseManager()->users();
    f_context.reply("All users:\n" + l_users.join("\n"));
}

static void handleLogout(CommandContext &f_context)
{
    if (!f_context.isAuthenticated()) {
        f_context.reply("You are not logged in!");
        return;
    }

    f_context.setAuthenticated(false);
    f_context.setAclRoleId(QString());
    f_context.setModeratorName(QString());
    f_context.sendPacket(QStringLiteral("AUTH"), {QStringLiteral("-1")});
}

static void handleChangePassword(CommandContext &f_context)
{
    QString l_username;
    QString l_password = f_context.argument(0);

    if (f_context.argc() == 1) {
        if (f_context.moderatorName().isEmpty()) {
            f_context.reply("You do not have permission to use that command. You must be logged in.");
            return;
        }
        l_username = f_context.moderatorName();
    }
    else if (f_context.argc() == 2) {
        if (f_context.canPerform(akashi::permission::super)) {
            l_username = f_context.argument(1);
        }
        else {
            f_context.reply("You do not have permission to use that command.");
            return;
        }
    }
    else {
        f_context.reply("Invalid command syntax.");
        return;
    }

    if (!checkPasswordRequirements(f_context.server()->serverSettings(), l_username, l_password)) {
        f_context.reply("Password does not meet server requirements.");
        return;
    }

    if (f_context.server()->databaseManager()->updatePassword(l_username, l_password))
        f_context.reply("Successfully changed password.");
    else
        f_context.reply("There was an error changing the password.");
}

void registerAuthenticationCommands(CommandRegistry &f_registry)
{
    f_registry.registerCommand(
        {QStringLiteral("login"), {}, {}, 0,
         QStringLiteral("/login"),
         QStringLiteral("Begins the login process.")},
        handleLogin, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("changeauth"), {}, {QStringLiteral("super")}, 0,
         QStringLiteral("/changeauth"),
         QStringLiteral("Switches the server authentication type from simple to advanced.")},
        handleChangeAuth, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("rootpass"), {}, {QStringLiteral("super")}, 1,
         QStringLiteral("/rootpass <password>"),
         QStringLiteral("Sets the root password after /changeauth.")},
        handleSetRootPass, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("adduser"), {}, {QStringLiteral("modify_users")}, 2,
         QStringLiteral("/adduser <username> <password>"),
         QStringLiteral("Creates a new user with the given credentials.")},
        handleAddUser, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("removeuser"), {}, {QStringLiteral("modify_users")}, 1,
         QStringLiteral("/removeuser <username>"),
         QStringLiteral("Removes the user with the given name.")},
        handleRemoveUser, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("listperms"), {}, {}, 0,
         QStringLiteral("/listperms [username]"),
         QStringLiteral("Lists your permissions, or the permissions of the given user.")},
        handleListPerms, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("setperms"), {}, {QStringLiteral("modify_users")}, 2,
         QStringLiteral("/setperms <username> <role>"),
         QStringLiteral("Sets the role of the given user.")},
        handleSetPerms, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("removeperms"), {}, {QStringLiteral("modify_users")}, 1,
         QStringLiteral("/removeperms <username>"),
         QStringLiteral("Removes all permissions from the given user.")},
        handleRemovePerms, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("listusers"), {}, {QStringLiteral("modify_users")}, 0,
         QStringLiteral("/listusers"),
         QStringLiteral("Lists all users in the database.")},
        handleListUsers, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("logout"), {}, {}, 0,
         QStringLiteral("/logout"),
         QStringLiteral("Logs you out.")},
        handleLogout, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("changepass"), {}, {}, 1,
         QStringLiteral("/changepass <password> [username]"),
         QStringLiteral("Changes your password, or another user's password if you have super permissions.")},
        handleChangePassword, QStringLiteral("core"));
}

} // namespace akashi::commands
