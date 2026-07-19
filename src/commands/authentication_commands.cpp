#include "commands/authentication_commands.h"

#include "core/command_context.h"
#include "core/command_registry.h"
#include "core/crypto_helper.h"
#include "core/db_manager.h"
#include "core/permission_registry.h"
#include "core/server_context.h"
#include "core/server_settings.h"

#include <QRegularExpression>
#include <QSet>

namespace akashi::commands {

static QString decodeMessage(const QString &f_message)
{
    return QString(f_message)
        .replace("<num>", "#")
        .replace("<percent>", "%")
        .replace("<dollar>", "$")
        .replace("<and>", "&");
}

bool passwordMeetsRequirements(ServerSettings *f_settings, const QString &f_username, const QString &f_password)
{
    if (!f_settings->password_requirements())
        return true;

    if (f_settings->pass_min_length() > f_password.size())
        return false;

    if (f_settings->pass_max_length() < f_password.size() && f_settings->pass_max_length() != 0)
        return false;

    if (f_settings->pass_required_mix_case()) {
        if (f_password.toLower() == f_password)
            return false;
        if (f_password.toUpper() == f_password)
            return false;
    }

    if (f_settings->pass_required_numbers()) {
        static const QRegularExpression s_numbers(QStringLiteral("[0123456789]"));
        QRegularExpressionMatch l_match = s_numbers.match(f_password);
        if (!l_match.hasMatch())
            return false;
    }

    if (f_settings->pass_required_special()) {
        static const QRegularExpression s_special(QStringLiteral("[~!@#$%^&*_\\-+=`|\\\\(){}\\[\\]:;\"'<>,.?/]"));
        QRegularExpressionMatch l_match = s_special.match(f_password);
        if (!l_match.hasMatch())
            return false;
    }

    if (!f_settings->pass_can_contain_username()) {
        if (f_password.contains(f_username))
            return false;
    }

    return true;
}

// The command-facing form: OOC packets escape special characters, so the
// password is decoded before the checks.
static bool checkPasswordRequirements(ServerSettings *f_settings, const QString &f_username, const QString &f_password)
{
    return passwordMeetsRequirements(f_settings, f_username, decodeMessage(f_password));
}

void cmdLogin(CommandContext &f_context)
{
    if (f_context.isAuthenticated()) {
        f_context.reply("You are already logged in!");
        return;
    }

    const QString l_system_id = f_context.server()->activeAuthSystemId();
    if (l_system_id == QStringLiteral("password")) {
        if (f_context.server()->serverSettings()->modpass().isEmpty()) {
            f_context.reply("No modpass is set. Please set a modpass before logging in.");
            return;
        }
        f_context.reply("Entering login prompt.\nPlease enter the server modpass.");
        f_context.setInLoginPrompt(true);
        return;
    }
    if (l_system_id == QStringLiteral("username")) {
        f_context.reply("Entering login prompt.\nPlease enter your username and password.");
        f_context.setInLoginPrompt(true);
        return;
    }

    // The prompt only speaks the core dialects; other systems log in
    // through the AUTH packet.
    f_context.reply("This server uses " + l_system_id + " authentication.");
}

void cmdAddUser(CommandContext &f_context)
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

void cmdRemoveUser(CommandContext &f_context)
{
    if (f_context.server()->databaseManager()->deleteUser(f_context.argument(0)))
        f_context.reply("Successfully removed user " + f_context.argument(0) + ".");
    else
        f_context.reply("Unable to remove user " + f_context.argument(0) + ".\nDoes it exist?");
}

static void replyRolePermissions(CommandContext &f_context, const QString &f_header, const ACLRole &f_role)
{
    QStringList l_message{f_header};

    const QSet<QString> &l_permissions = f_role.permissions();
    if (l_permissions.isEmpty()) {
        l_message.append("NONE");
    }
    else if (f_role.canPerform(permission::super)) {
        l_message.append("SUPER (Be careful! This grants the user all permissions.)");
    }
    else {
        QStringList l_sorted(l_permissions.begin(), l_permissions.end());
        l_sorted.sort();
        l_message.append(l_sorted);
    }

    f_context.reply(l_message.join("\n"));
}

void cmdListPermsSelf(CommandContext &f_context)
{
    const ACLRole l_role = f_context.server()->aclRolesHandler()->roleById(f_context.aclRoleId());
    replyRolePermissions(f_context, "You have been given the following permissions:", l_role);
}

void cmdListPermsUser(CommandContext &f_context)
{
    const QString l_username = f_context.argument(0);
    const QString l_acl_id = f_context.server()->databaseManager()->acl(l_username);
    if (l_acl_id.isEmpty()) {
        f_context.reply(l_username + " wasn't found!");
        return;
    }

    const ACLRole l_role = f_context.server()->aclRolesHandler()->roleById(l_acl_id);
    replyRolePermissions(f_context, "User " + l_username + " has the following permissions:", l_role);
}

void cmdSetPerms(CommandContext &f_context)
{
    const QString l_target_acl = f_context.argument(1);
    if (!f_context.server()->aclRolesHandler()->roleExists(l_target_acl)) {
        f_context.reply("That role doesn't exist!");
        return;
    }

    // Handing out SUPER is the spec-declared escalation.
    if (l_target_acl == ACLRolesHandler::SUPER_ID && !f_context.canPerform(f_context.escalatesTo())) {
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

void cmdRemovePerms(CommandContext &f_context)
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

void cmdListUsers(CommandContext &f_context)
{
    QStringList l_users = f_context.server()->databaseManager()->users();
    f_context.reply("All users:\n" + l_users.join("\n"));
}

void cmdLogout(CommandContext &f_context)
{
    if (!f_context.isAuthenticated()) {
        f_context.reply("You are not logged in!");
        return;
    }

    f_context.setAuthenticated(false);
    f_context.setAclRoleId(QString());
    f_context.setModeratorName(QString());
    f_context.sendPacket(QStringLiteral("AUTH"), {QStringLiteral("-1")});
    f_context.reply("Logged out.");
}

static void changePassword(CommandContext &f_context, const QString &f_username, const QString &f_password)
{
    if (!checkPasswordRequirements(f_context.server()->serverSettings(), f_username, f_password)) {
        f_context.reply("Password does not meet server requirements.");
        return;
    }

    if (f_context.server()->databaseManager()->updatePassword(f_username, f_password))
        f_context.reply("Successfully changed password.");
    else
        f_context.reply("There was an error changing the password.");
}

void cmdChangePassSelf(CommandContext &f_context)
{
    if (f_context.moderatorName().isEmpty()) {
        f_context.reply("You do not have permission to use that command. You must be logged in.");
        return;
    }
    changePassword(f_context, f_context.moderatorName(), f_context.argument(0));
}

void cmdChangePassUser(CommandContext &f_context)
{
    changePassword(f_context, f_context.argument(1), f_context.argument(0));
}

void registerAuthenticationCommands(CommandRegistry &f_registry)
{
    f_registry.registerCommand(
        {QStringLiteral("login"), {}, {akashi::permission::user}, 0, QStringLiteral("/login"), QStringLiteral("Begins the login process.")},
        cmdLogin, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("adduser"), {}, {akashi::permission::modify_users}, 2, QStringLiteral("/adduser <username> <password>"), QStringLiteral("Creates a new user with the given credentials."), 1},
        cmdAddUser, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("removeuser"), {}, {akashi::permission::modify_users}, 1, QStringLiteral("/removeuser <username>"), QStringLiteral("Removes the user with the given name.")},
        cmdRemoveUser, QStringLiteral("core"));

    CommandSpec l_listperms;
    l_listperms.name = QStringLiteral("listperms");
    l_listperms.usage = QStringLiteral("/listperms [username]");
    l_listperms.description = QStringLiteral("Lists your permissions, or the permissions of the given user.");
    l_listperms.variants = {
        {QStringLiteral("own"), 0, 0, {akashi::permission::user}, QStringLiteral("/listperms"), QStringLiteral("Lists your permissions."), cmdListPermsSelf},
        {QStringLiteral("user"), 1, -1, {akashi::permission::modify_users}, QStringLiteral("/listperms <username>"), QStringLiteral("Lists the given user's permissions."), cmdListPermsUser},
    };
    f_registry.registerCommand(l_listperms, QStringLiteral("core"));

    {
        CommandSpec l_setperms;
        l_setperms.name = QStringLiteral("setperms");
        l_setperms.permissions = {akashi::permission::modify_users};
        l_setperms.min_args = 2;
        l_setperms.usage = QStringLiteral("/setperms <username> <role>");
        l_setperms.description = QStringLiteral("Sets the role of the given user.");
        l_setperms.escalates_to = akashi::permission::super;
        l_setperms.escalates_when = QStringLiteral("assigning the SUPER role");
        f_registry.registerCommand(l_setperms, cmdSetPerms, QStringLiteral("core"));
    }

    f_registry.registerCommand(
        {QStringLiteral("removeperms"), {}, {akashi::permission::modify_users}, 1, QStringLiteral("/removeperms <username>"), QStringLiteral("Removes all permissions from the given user.")},
        cmdRemovePerms, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("listusers"), {}, {akashi::permission::modify_users}, 0, QStringLiteral("/listusers"), QStringLiteral("Lists all users in the database.")},
        cmdListUsers, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("logout"), {}, {akashi::permission::user}, 0, QStringLiteral("/logout"), QStringLiteral("Logs you out.")},
        cmdLogout, QStringLiteral("core"));

    CommandSpec l_changepass;
    l_changepass.name = QStringLiteral("changepass");
    l_changepass.usage = QStringLiteral("/changepass <password> [username]");
    l_changepass.description = QStringLiteral("Changes your password, or another user's password if you have super permissions.");
    l_changepass.sensitive_args_from = 0;
    l_changepass.variants = {
        {QStringLiteral("own"), 1, 1, {akashi::permission::user}, QStringLiteral("/changepass <password>"), QStringLiteral("Changes your password."), cmdChangePassSelf},
        {QStringLiteral("user"), 2, 2, {akashi::permission::super}, QStringLiteral("/changepass <password> <username>"), QStringLiteral("Changes another user's password."), cmdChangePassUser},
    };
    f_registry.registerCommand(l_changepass, QStringLiteral("core"));
}

} // namespace akashi::commands
