#include "commands/moderation_commands.h"

#include "akashi/permissions.h"
#include "aoclient.h"
#include "area_data.h"
#include "command_extension.h"
#include "config_manager.h"
#include "core/command_context.h"
#include "core/command_registry.h"
#include "core/command_spec.h"
#include "db_manager.h"
#include "server.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QRegularExpression>

namespace akashi::commands {

static long long parseTime(const QString &f_input)
{
    QRegularExpression l_regex("(?:(?:(?<year>.*?)y)*(?:(?<week>.*?)w)*(?:(?<day>.*?)d)*(?:(?<hr>.*?)h)*(?:(?<min>.*?)m)*(?:(?<sec>.*?)s)*)");
    QRegularExpressionMatch match = l_regex.match(f_input);
    QString str_year, str_week, str_hour, str_day, str_minute, str_second;
    int year, week, day, hour, minute, second;

    str_year = match.captured("year");
    str_week = match.captured("week");
    str_day = match.captured("day");
    str_hour = match.captured("hr");
    str_minute = match.captured("min");
    str_second = match.captured("sec");

    bool l_is_well_formed = false;
    QString concat_str(str_year + str_week + str_day + str_hour + str_minute + str_second);
    concat_str.toInt(&l_is_well_formed);

    if (!l_is_well_formed) {
        return -1;
    }

    year = str_year.toInt();
    week = str_week.toInt();
    day = str_day.toInt();
    hour = str_hour.toInt();
    minute = str_minute.toInt();
    second = str_second.toInt();

    long long l_total = 0;
    l_total += 31622400 * year;
    l_total += 604800 * week;
    l_total += 86400 * day;
    l_total += 3600 * hour;
    l_total += 60 * minute;
    l_total += second;

    if (l_total < 0)
        return -1;

    return l_total;
}

static QString reprimand(bool f_positive = false)
{
    if (f_positive) {
        return ConfigManager::praiseList().at(CommandContext::genRand(0, ConfigManager::praiseList().size() - 1));
    }
    else {
        return ConfigManager::reprimandsList().at(CommandContext::genRand(0, ConfigManager::reprimandsList().size() - 1));
    }
}

static void sendNotice(CommandContext &f_context, const QString &f_notice, bool f_global = false)
{
    QString l_message = "A moderator sent this ";
    if (f_global)
        l_message += "server-wide ";
    l_message += "notice:\n\n" + f_notice;
    f_context.replyToArea(l_message);
    akashi::Packet l_packet("BB", {l_message});
    if (f_global)
        f_context.server()->broadcast(l_packet);
    else
        f_context.server()->broadcast(l_packet, f_context.areaId());
}

static void handleBan(CommandContext &f_context)
{
    QString l_args_str = f_context.argument(2);
    if (f_context.argc() > 3) {
        for (int i = 3; i < f_context.argc(); i++)
            l_args_str += " " + f_context.argument(i);
    }

    DBManager::BanInfo l_ban;

    long long l_duration_seconds = 0;
    if (f_context.argument(1) == "perma")
        l_duration_seconds = -2;
    else
        l_duration_seconds = parseTime(f_context.argument(1));

    if (l_duration_seconds == -1) {
        f_context.reply("Invalid time format. Format example: 1h30m");
        return;
    }

    l_ban.duration = l_duration_seconds;
    l_ban.ipid = f_context.argument(0);
    l_ban.reason = l_args_str;
    l_ban.time = QDateTime::currentDateTime().toSecsSinceEpoch();
    bool l_ban_logged = false;
    int l_kick_counter = 0;

    switch (ConfigManager::authType()) {
    case DataTypes::AuthType::SIMPLE:
        l_ban.moderator = "moderator";
        break;
    case DataTypes::AuthType::ADVANCED:
        l_ban.moderator = f_context.moderatorName();
        break;
    }

    const QList<AOClient *> l_targets = f_context.server()->clientsByIpid(l_ban.ipid);
    for (AOClient *l_client : l_targets) {
        if (!l_ban_logged) {
            l_ban.ip = l_client->remoteIp();
            l_ban.hdid = l_client->hwid();
            f_context.server()->databaseManager()->addBan(l_ban);
            f_context.reply("Banned user with ipid " + l_ban.ipid + " for reason: " + l_ban.reason);
            l_ban_logged = true;
        }
        QString l_ban_duration;
        if (!(l_ban.duration == -2)) {
            l_ban_duration = QDateTime::fromSecsSinceEpoch(l_ban.time).addSecs(l_ban.duration).toString("MM/dd/yyyy, hh:mm");
        }
        else {
            l_ban_duration = "Permanently.";
        }
        int l_ban_id = f_context.server()->databaseManager()->banId(l_ban.ip);
        l_client->sendPacket("KB", {l_ban.reason + "\nID: " + QString::number(l_ban_id) + "\nUntil: " + l_ban_duration});
        l_client->closeSocket();
        l_kick_counter++;

        AOClient *l_self = f_context.server()->clientById(f_context.clientId());
        Q_EMIT l_self->logBan(l_ban.moderator, l_ban.ipid, l_ban_duration, l_ban.reason);
        if (ConfigManager::discordBanWebhookEnabled())
            Q_EMIT f_context.server()->banWebhookRequest(l_ban.ipid, l_ban.moderator, l_ban_duration, l_ban.reason, l_ban_id);
    }

    if (l_kick_counter > 1)
        f_context.reply("Kicked " + QString::number(l_kick_counter) + " clients with matching ipids.");

    if (!l_ban_logged) {
        f_context.server()->databaseManager()->addBan(l_ban);
        f_context.reply("Banned " + l_ban.ipid + " for reason: " + l_ban.reason);
    }
}

static void handleKick(CommandContext &f_context)
{
    QString l_target_ipid = f_context.argument(0);
    QString l_reason = f_context.argument(1);
    int l_kick_counter = 0;

    if (f_context.argc() > 2) {
        for (int i = 2; i < f_context.argc(); i++) {
            l_reason += " " + f_context.argument(i);
        }
    }

    const QList<AOClient *> l_targets = f_context.server()->clientsByIpid(l_target_ipid);
    for (AOClient *l_client : l_targets) {
        l_client->sendPacket("KK", {l_reason});
        l_client->closeSocket();
        l_kick_counter++;
    }

    if (l_kick_counter > 0) {
        AOClient *l_self = f_context.server()->clientById(f_context.clientId());
        if (ConfigManager::authType() == DataTypes::AuthType::ADVANCED) {
            Q_EMIT l_self->logKick(f_context.moderatorName(), l_target_ipid, l_reason);
        }
        else {
            Q_EMIT l_self->logKick("Moderator", l_target_ipid, l_reason);
        }
        f_context.reply("Kicked " + QString::number(l_kick_counter) + " client(s) with ipid " + l_target_ipid + " for reason: " + l_reason);
    }
    else
        f_context.reply("User with ipid not found!");
}

static void handleMods(CommandContext &f_context)
{
    QStringList l_entries;
    int l_online_count = 0;
    const QVector<AOClient *> l_clients = f_context.server()->clients();
    for (AOClient *l_client : l_clients) {
        if (l_client->isAuthenticated()) {
            l_entries << "---";
            if (ConfigManager::authType() != DataTypes::AuthType::SIMPLE) {
                l_entries << "Moderator: " + l_client->moderatorName();
                l_entries << "Role:" << l_client->aclRoleId();
            }
            l_entries << "OOC name: " + l_client->name();
            l_entries << "ID: " + QString::number(l_client->clientId());
            l_entries << "Area: " + QString::number(l_client->areaId());
            l_entries << "Character: " + l_client->character();
            l_online_count++;
        }
    }
    l_entries << "---";
    l_entries << "Total online: " << QString::number(l_online_count);
    f_context.reply(l_entries.join("\n"));
}

static void handleCommands(CommandContext &f_context)
{
    QStringList l_entries;
    l_entries << "Allowed commands:";

    CommandRegistry *l_registry = f_context.server()->commandRegistry();
    for (const QString &l_name : l_registry->commandNames()) {
        auto l_spec = l_registry->spec(l_name);
        if (!l_spec)
            continue;
        bool l_allowed = l_spec->permissions.isEmpty();
        for (const QString &l_perm : l_spec->permissions) {
            if (f_context.canPerform(l_perm)) {
                l_allowed = true;
                break;
            }
        }
        if (l_allowed) {
            QString l_info = "/" + l_name;
            if (!l_spec->aliases.isEmpty()) {
                l_info += " [aka: " + l_spec->aliases.join(", ") + "]";
            }
            l_entries << l_info;
        }
    }

    CommandExtensionCollection *l_extensions = f_context.server()->commandExtensionCollection();
    AOClient *l_self = f_context.server()->clientById(f_context.clientId());
    QMap<QString, AOClient::CommandInfo>::const_iterator i;
    for (i = AOClient::COMMANDS.constBegin(); i != AOClient::COMMANDS.constEnd(); ++i) {
        const AOClient::CommandInfo l_command = i.value();
        const CommandExtension l_extension = l_extensions->extension(i.key());
        const QVector<ACLRole::Permission> l_permissions = l_extension.permissions(l_command.acl_permissions);
        bool l_has_permission = false;
        for (const ACLRole::Permission i_permission : qAsConst(l_permissions)) {
            if (l_self->canPerform(i_permission)) {
                l_has_permission = true;
                break;
            }
        }
        if (!l_has_permission) {
            continue;
        }

        QString l_info = "/" + i.key();
        const QStringList l_aliases = l_extension.aliases();
        if (!l_aliases.isEmpty()) {
            l_info += " [aka: " + l_aliases.join(", ") + "]";
        }
        l_entries << l_info;
    }
    f_context.reply(l_entries.join("\n"));
}

static void handleHelp(CommandContext &f_context)
{
    CommandExtensionCollection *l_extension_collection = f_context.server()->commandExtensionCollection();
    CommandRegistry *l_registry = f_context.server()->commandRegistry();
    AOClient *l_self = f_context.server()->clientById(f_context.clientId());

    if (f_context.argc() == 0) {
        f_context.reply("Type /help <command> for help on a specific command, or /help all to list all commands.");
        return;
    }

    if (f_context.argc() > 1) {
        f_context.reply("Too many arguments. Please only use the command name.");
        return;
    }

    QString l_command_name = f_context.argument(0).toLower();

    auto l_check_legacy_permission = [l_self, l_extension_collection](const QString &f_command_name) -> bool {
        const QVector<ACLRole::Permission> l_permissions = l_extension_collection->extension(f_command_name).permissions(AOClient::COMMANDS.value(f_command_name).acl_permissions);
        for (const ACLRole::Permission i_permission : l_permissions) {
            if (l_self->canPerform(i_permission)) {
                return true;
            }
        }
        return false;
    };

    auto l_format_legacy_command = [l_extension_collection](const QString &f_command_name) -> QString {
        QString l_display_name = f_command_name;
        if (l_extension_collection->containsExtension(f_command_name)) {
            l_display_name = l_extension_collection->extension(f_command_name).displayName();
        }

        const QString l_description = ConfigManager::commandHelp(f_command_name).text;
        return "/" + l_display_name + "\n" + (l_description.isEmpty() ? QString("No details available.") : l_description);
    };

    auto l_check_registry_permission = [&f_context, l_registry](const QString &f_command_name) -> bool {
        auto l_spec = l_registry->spec(f_command_name);
        if (!l_spec)
            return false;
        if (l_spec->permissions.isEmpty())
            return true;
        for (const QString &l_perm : l_spec->permissions) {
            if (f_context.canPerform(l_perm))
                return true;
        }
        return false;
    };

    auto l_format_registry_command = [l_registry](const QString &f_command_name) -> QString {
        auto l_spec = l_registry->spec(f_command_name);
        if (!l_spec)
            return {};
        QString l_text = "/" + l_spec->name;
        if (!l_spec->usage.isEmpty())
            l_text = l_spec->usage;
        l_text += "\n" + (l_spec->description.isEmpty() ? QString("No details available.") : l_spec->description);
        return l_text;
    };

    QString l_message = "==Help==\n";

    if (l_command_name == "all") {
        QStringList l_entries;
        for (const QString &l_name : l_registry->commandNames()) {
            if (l_check_registry_permission(l_name)) {
                l_entries.append(l_format_registry_command(l_name));
            }
        }
        for (auto it = AOClient::COMMANDS.cbegin(); it != AOClient::COMMANDS.cend(); ++it) {
            if (l_check_legacy_permission(it.key())) {
                l_entries.append(l_format_legacy_command(it.key()));
            }
        }
        f_context.reply(l_message + l_entries.join("\n\n"));
        return;
    }

    if (l_extension_collection->containsExtension(l_command_name)) {
        l_command_name = l_extension_collection->extension(l_command_name).commandName();
    }

    if (l_registry->contains(l_command_name)) {
        if (!l_check_registry_permission(l_command_name)) {
            f_context.reply(l_message + "You are not allowed to use the command " + l_command_name + ".");
            return;
        }
        f_context.reply(l_message + l_format_registry_command(l_command_name));
        return;
    }

    if (!AOClient::COMMANDS.contains(l_command_name)) {
        f_context.reply(l_message + "Unable to find the command " + l_command_name + ".");
        return;
    }

    if (!l_check_legacy_permission(l_command_name)) {
        f_context.reply(l_message + "You are not allowed to use the command " + l_command_name + ".");
        return;
    }

    f_context.reply(l_message + l_format_legacy_command(l_command_name));
}

static void handleMotd(CommandContext &f_context)
{
    f_context.reply("=== MOTD ===\r\n" + ConfigManager::motd() + "\r\n=============");
}

static void handleSetMotd(CommandContext &f_context)
{
    QString l_motd = f_context.arguments().join(" ");
    ConfigManager::setMotd(l_motd);
    f_context.reply("MOTD has been changed.");
}

static void handleBans(CommandContext &f_context)
{
    QStringList l_recent_bans;
    l_recent_bans << "Last 5 bans:";
    l_recent_bans << "-----";
    const QList<DBManager::BanInfo> l_bans_list = f_context.server()->databaseManager()->recentBans();
    for (const DBManager::BanInfo &l_ban : l_bans_list) {
        QString l_banned_until;
        if (l_ban.duration == -2)
            l_banned_until = "The heat death of the universe";
        else
            l_banned_until = QDateTime::fromSecsSinceEpoch(l_ban.time).addSecs(l_ban.duration).toString("MM/dd/yyyy, hh:mm");
        l_recent_bans << "Ban ID: " + QString::number(l_ban.id);
        l_recent_bans << "Affected IPID: " + l_ban.ipid;
        l_recent_bans << "Affected HDID: " + l_ban.hdid;
        l_recent_bans << "Reason for ban: " + l_ban.reason;
        l_recent_bans << "Date of ban: " + QDateTime::fromSecsSinceEpoch(l_ban.time).toString("MM/dd/yyyy, hh:mm");
        l_recent_bans << "Ban lasts until: " + l_banned_until;
        l_recent_bans << "Moderator: " + l_ban.moderator;
        l_recent_bans << "-----";
    }
    f_context.reply(l_recent_bans.join("\n"));
}

static void handleUnban(CommandContext &f_context)
{
    bool ok;
    int l_target_ban = f_context.argument(0).toInt(&ok);
    if (!ok) {
        f_context.reply("Invalid ban ID.");
        return;
    }
    else if (f_context.server()->databaseManager()->invalidateBan(l_target_ban))
        f_context.reply("Successfully invalidated ban " + f_context.argument(0) + ".");
    else
        f_context.reply("Couldn't invalidate ban " + f_context.argument(0) + ", are you sure it exists?");
}

static void handleAbout(CommandContext &f_context)
{
    f_context.sendPacket("CT", {"The akashi dev team", "Thank you for using akashi! Made with love by scatterflower, with help from in1tiate, Salanto, and mangosarentliterature. akashi " + QCoreApplication::applicationVersion() + ". For documentation and reporting issues, see the source: https://github.com/AttorneyOnline/akashi"});
}

static void handleMute(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        if (l_target->hasSanction(akashi::sanction::muted))
            f_context.reply("That player is already muted!");
        else {
            f_context.reply("Muted player.");
            l_target->reply("You were muted by a moderator. " + reprimand());
        }
        l_target->setSanction(akashi::sanction::muted, true);
    }
}

static void handleUnmute(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        if (!l_target->hasSanction(akashi::sanction::muted))
            f_context.reply("That player is not muted!");
        else {
            f_context.reply("Unmuted player.");
            l_target->reply("You were unmuted by a moderator. " + reprimand(true));
        }
        l_target->setSanction(akashi::sanction::muted, false);
    }
}

static void handleOocMute(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        if (l_target->hasSanction(akashi::sanction::ooc_muted))
            f_context.reply("That player is already OOC muted!");
        else {
            f_context.reply("OOC muted player.");
            l_target->reply("You were OOC muted by a moderator. " + reprimand());
        }
        l_target->setSanction(akashi::sanction::ooc_muted, true);
    }
}

static void handleOocUnmute(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        if (!l_target->hasSanction(akashi::sanction::ooc_muted))
            f_context.reply("That player is not OOC muted!");
        else {
            f_context.reply("OOC unmuted player.");
            l_target->reply("You were OOC unmuted by a moderator. " + reprimand(true));
        }
        l_target->setSanction(akashi::sanction::ooc_muted, false);
    }
}

static void handleBlockWtce(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        if (l_target->hasSanction(akashi::sanction::wtce_blocked))
            f_context.reply("That player is already judge blocked!");
        else {
            f_context.reply("Revoked player's access to judge controls.");
            l_target->reply("A moderator revoked your judge controls access. " + reprimand());
        }
        l_target->setSanction(akashi::sanction::wtce_blocked, true);
    }
}

static void handleUnblockWtce(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        if (!l_target->hasSanction(akashi::sanction::wtce_blocked))
            f_context.reply("That player is not judge blocked!");
        else {
            f_context.reply("Restored player's access to judge controls.");
            l_target->reply("A moderator restored your judge controls access. " + reprimand(true));
        }
        l_target->setSanction(akashi::sanction::wtce_blocked, false);
    }
}

static void handleAllowBlankposting(CommandContext &f_context)
{
    AreaData *l_area = f_context.server()->areaById(f_context.areaId());
    l_area->toggleBlankposting();
    if (l_area->isBlankpostingAllowed() == false) {
        f_context.replyToArea(f_context.name() + " has set blankposting in the area to forbidden.");
    }
    else {
        f_context.replyToArea(f_context.name() + " has set blankposting in the area to allowed.");
    }
}

static void handleBanInfo(CommandContext &f_context)
{
    QStringList l_ban_info;
    l_ban_info << ("Ban Info for " + f_context.argument(0));
    l_ban_info << "-----";
    QString l_lookup_type;

    if (f_context.argc() == 1) {
        l_lookup_type = "banid";
    }
    else if (f_context.argc() == 2) {
        l_lookup_type = f_context.argument(1);
        if (!((l_lookup_type == "banid") || (l_lookup_type == "ipid") || (l_lookup_type == "hdid"))) {
            f_context.reply("Invalid ID type.");
            return;
        }
    }
    else {
        f_context.reply("Invalid command.");
        return;
    }
    QString l_id = f_context.argument(0);
    const QList<DBManager::BanInfo> l_bans = f_context.server()->databaseManager()->banInfo(l_lookup_type, l_id);
    for (const DBManager::BanInfo &l_ban : l_bans) {
        QString l_banned_until;
        if (l_ban.duration == -2)
            l_banned_until = "The heat death of the universe";
        else
            l_banned_until = QDateTime::fromSecsSinceEpoch(l_ban.time).addSecs(l_ban.duration).toString("MM/dd/yyyy, hh:mm");
        l_ban_info << "Ban ID: " + QString::number(l_ban.id);
        l_ban_info << "Affected IPID: " + l_ban.ipid;
        l_ban_info << "Affected HDID: " + l_ban.hdid;
        l_ban_info << "Reason for ban: " + l_ban.reason;
        l_ban_info << "Date of ban: " + QDateTime::fromSecsSinceEpoch(l_ban.time).toString("MM/dd/yyyy, hh:mm");
        l_ban_info << "Ban lasts until: " + l_banned_until;
        l_ban_info << "Moderator: " + l_ban.moderator;
        l_ban_info << "-----";
    }
    f_context.reply(l_ban_info.join("\n"));
}

static void handleReload(CommandContext &f_context)
{
    f_context.server()->reloadSettings();
    f_context.reply("Reloaded configurations");
}

static void handleForceImmediate(CommandContext &f_context)
{
    AreaData *l_area = f_context.server()->areaById(f_context.areaId());
    l_area->toggleImmediate();
    QString l_state = l_area->forceImmediate() ? "on." : "off.";
    f_context.reply("Forced immediate text processing in this area is now " + l_state);
}

static void handleAllowIniswap(CommandContext &f_context)
{
    AreaData *l_area = f_context.server()->areaById(f_context.areaId());
    l_area->toggleIniswap();
    QString state = l_area->isIniswapAllowed() ? "allowed." : "disallowed.";
    f_context.reply("Iniswapping in this area is now " + state);
}

static void handlePermitSaving(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        l_target->setTestimonySaving(true);
        f_context.reply("Testimony saving has been enabled for client " + QString::number(l_target->clientId()));
    }
}

static void handleKickUid(CommandContext &f_context)
{
    QString l_reason = f_context.argument(1);

    if (f_context.argc() > 2) {
        for (int i = 2; i < f_context.argc(); i++) {
            l_reason += " " + f_context.argument(i);
        }
    }

    if (auto l_target = f_context.resolveTarget()) {
        l_target->sendPacket("KK", {l_reason});
        l_target->closeSocket();
        f_context.reply("Kicked client with UID " + f_context.argument(0) + " for reason: " + l_reason);
    }
}

static void handleUpdateBan(CommandContext &f_context)
{
    bool conv_ok = false;
    int l_ban_id = f_context.argument(0).toInt(&conv_ok);
    if (!conv_ok) {
        f_context.reply("Invalid ban ID.");
        return;
    }
    QVariant l_updated_info;
    if (f_context.argument(1) == "duration") {
        long long l_duration_seconds = 0;
        if (f_context.argument(2) == "perma")
            l_duration_seconds = -2;
        else
            l_duration_seconds = parseTime(f_context.argument(2));
        if (l_duration_seconds == -1) {
            f_context.reply("Invalid time format. Format example: 1h30m");
            return;
        }
        l_updated_info = QVariant(l_duration_seconds);
    }
    else if (f_context.argument(1) == "reason") {
        QString l_args_str = f_context.argument(2);
        if (f_context.argc() > 3) {
            for (int i = 3; i < f_context.argc(); i++)
                l_args_str += " " + f_context.argument(i);
        }
        l_updated_info = QVariant(l_args_str);
    }
    else {
        f_context.reply("Invalid update type.");
        return;
    }
    if (!f_context.server()->databaseManager()->updateBan(l_ban_id, f_context.argument(1), l_updated_info)) {
        f_context.reply("There was an error updating the ban. Please confirm the ban ID is valid.");
        return;
    }
    f_context.reply("Ban updated.");
}

static void handleNotice(CommandContext &f_context)
{
    sendNotice(f_context, f_context.arguments().join(" "));
}

static void handleNoticeGlobal(CommandContext &f_context)
{
    sendNotice(f_context, f_context.arguments().join(" "), true);
}

static void handleKickOther(CommandContext &f_context)
{
    int l_kick_counter = 0;

    QList<AOClient *> l_target_clients;
    const QList<AOClient *> l_targets_hwid = f_context.server()->clientsByHwid(f_context.hwid());
    l_target_clients = f_context.server()->clientsByIpid(f_context.ipid());

    for (AOClient *l_target_candidate : qAsConst(l_targets_hwid)) {
        if (!l_target_clients.contains(l_target_candidate)) {
            l_target_clients.append(l_target_candidate);
        }
    }

    QMutableListIterator<AOClient *> it(l_target_clients);
    while (it.hasNext()) {
        if (it.next()->clientId() == f_context.clientId()) {
            it.remove();
        }
    }

    for (AOClient *l_target_client : qAsConst(l_target_clients)) {
        l_target_client->closeSocket();
        l_kick_counter++;
    }
    f_context.reply("Kicked " + QString::number(l_kick_counter) + " multiclients from the server.");
}

void registerModerationCommands(CommandRegistry &f_registry)
{
    f_registry.registerCommand(
        {QStringLiteral("ban"), {}, {akashi::permission::ban}, 3,
         QStringLiteral("/ban <ipid> <duration> <reason>"),
         QStringLiteral("Bans a client by IPID.")},
        handleBan, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("kick"), {}, {akashi::permission::kick}, 2,
         QStringLiteral("/kick <ipid> <reason>"),
         QStringLiteral("Kicks all clients with the given IPID.")},
        handleKick, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("mods"), {}, {}, 0,
         QStringLiteral("/mods"),
         QStringLiteral("Lists currently logged-in moderators.")},
        handleMods, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("commands"), {}, {}, 0,
         QStringLiteral("/commands"),
         QStringLiteral("Lists all commands available to you.")},
        handleCommands, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("help"), {}, {}, 0,
         QStringLiteral("/help [command|all]"),
         QStringLiteral("Displays help for a command.")},
        handleHelp, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("motd"), {}, {}, 0,
         QStringLiteral("/motd"),
         QStringLiteral("Displays the Message of the Day.")},
        handleMotd, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("set_motd"), {}, {akashi::permission::motd}, 1,
         QStringLiteral("/set_motd <message>"),
         QStringLiteral("Sets the Message of the Day.")},
        handleSetMotd, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("bans"), {}, {akashi::permission::ban}, 0,
         QStringLiteral("/bans"),
         QStringLiteral("Lists the last 5 bans.")},
        handleBans, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("unban"), {}, {akashi::permission::ban}, 1,
         QStringLiteral("/unban <ban_id>"),
         QStringLiteral("Invalidates a ban by its ID.")},
        handleUnban, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("about"), {}, {}, 0,
         QStringLiteral("/about"),
         QStringLiteral("Displays server version info.")},
        handleAbout, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("mute"), {}, {akashi::permission::mute}, 1,
         QStringLiteral("/mute <id>"),
         QStringLiteral("Mutes a client.")},
        handleMute, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("unmute"), {}, {akashi::permission::mute}, 1,
         QStringLiteral("/unmute <id>"),
         QStringLiteral("Unmutes a client.")},
        handleUnmute, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("ooc_mute"), {}, {akashi::permission::mute}, 1,
         QStringLiteral("/ooc_mute <id>"),
         QStringLiteral("OOC-mutes a client.")},
        handleOocMute, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("ooc_unmute"), {}, {akashi::permission::mute}, 1,
         QStringLiteral("/ooc_unmute <id>"),
         QStringLiteral("OOC-unmutes a client.")},
        handleOocUnmute, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("block_wtce"), {}, {akashi::permission::mute}, 1,
         QStringLiteral("/block_wtce <id>"),
         QStringLiteral("Blocks a client from using judge controls.")},
        handleBlockWtce, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("unblock_wtce"), {}, {akashi::permission::mute}, 1,
         QStringLiteral("/unblock_wtce <id>"),
         QStringLiteral("Restores a client's judge controls.")},
        handleUnblockWtce, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("allow_blankposting"), {}, {akashi::permission::chat_moderator}, 0,
         QStringLiteral("/allow_blankposting"),
         QStringLiteral("Toggles blankposting in the area.")},
        handleAllowBlankposting, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("baninfo"), {}, {akashi::permission::ban}, 1,
         QStringLiteral("/baninfo <id> [type]"),
         QStringLiteral("Looks up ban info.")},
        handleBanInfo, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("reload"), {}, {akashi::permission::super}, 0,
         QStringLiteral("/reload"),
         QStringLiteral("Reloads server configuration.")},
        handleReload, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("force_noint_pres"), {}, {akashi::permission::gamemaster}, 0,
         QStringLiteral("/force_noint_pres"),
         QStringLiteral("Toggles forced immediate text processing.")},
        handleForceImmediate, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("allow_iniswap"), {}, {akashi::permission::gamemaster}, 0,
         QStringLiteral("/allow_iniswap"),
         QStringLiteral("Toggles iniswap permission in the area.")},
        handleAllowIniswap, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("permitsaving"), {}, {akashi::permission::chat_moderator}, 1,
         QStringLiteral("/permitsaving <id>"),
         QStringLiteral("Grants a client permission to save testimony.")},
        handlePermitSaving, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("kick_uid"), {}, {akashi::permission::kick}, 2,
         QStringLiteral("/kick_uid <id> <reason>"),
         QStringLiteral("Kicks a single client by UID.")},
        handleKickUid, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("update_ban"), {}, {akashi::permission::ban}, 3,
         QStringLiteral("/update_ban <ban_id> <field> <value>"),
         QStringLiteral("Updates a ban's duration or reason.")},
        handleUpdateBan, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("notice"), {}, {akashi::permission::send_notice}, 1,
         QStringLiteral("/notice <message>"),
         QStringLiteral("Sends a notice to the area.")},
        handleNotice, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("noticeg"), {}, {akashi::permission::send_notice}, 1,
         QStringLiteral("/noticeg <message>"),
         QStringLiteral("Sends a server-wide notice.")},
        handleNoticeGlobal, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("kick_other"), {}, {}, 0,
         QStringLiteral("/kick_other"),
         QStringLiteral("Kicks your other connected clients.")},
        handleKickOther, QStringLiteral("core"));
}

} // namespace akashi::commands
