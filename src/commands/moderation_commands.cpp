#include "commands/moderation_commands.h"

#include "akashi/permissions.h"
#include "akashi/scheduler.h"
#include "core/client_session.h"
#include "core/command_context.h"
#include "core/command_registry.h"
#include "core/command_spec.h"
#include "core/db_manager.h"
#include "core/server_context.h"
#include "core/server_settings.h"
#include "world/area.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QRegularExpression>

#include <functional>
#include <utility>

namespace akashi::commands {

static long long parseTime(const QString &f_input)
{
    static const QRegularExpression s_duration("(?:(?:(?<year>.*?)y)*(?:(?<week>.*?)w)*(?:(?<day>.*?)d)*(?:(?<hr>.*?)h)*(?:(?<min>.*?)m)*(?:(?<sec>.*?)s)*)");
    QRegularExpressionMatch match = s_duration.match(f_input);
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

static QString reprimand(ServerContext *f_server, bool f_positive = false)
{
    if (f_positive) {
        return f_server->praiseList().at(CommandContext::genRand(0, f_server->praiseList().size() - 1));
    }
    else {
        return f_server->reprimandsList().at(CommandContext::genRand(0, f_server->reprimandsList().size() - 1));
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

// The /ban door: parses the duration and phrases the replies; the act
// itself is the server's ban verb. This door's KB carries the ban id and
// the until text.
void cmdBan(CommandContext &f_context)
{
    QString l_args_str = f_context.argument(2);
    if (f_context.argc() > 3) {
        for (int i = 3; i < f_context.argc(); i++)
            l_args_str += " " + f_context.argument(i);
    }

    long long l_duration_seconds = 0;
    if (f_context.argument(1) == "perma")
        l_duration_seconds = -2;
    else
        l_duration_seconds = parseTime(f_context.argument(1));

    if (l_duration_seconds == -1) {
        f_context.reply("Invalid time format. Format example: 1h30m");
        return;
    }

    QString l_moderator;
    switch (f_context.server()->authType()) {
    case AuthType::SIMPLE:
        l_moderator = "moderator";
        break;
    case AuthType::ADVANCED:
        l_moderator = f_context.moderatorName();
        break;
    }

    const QString l_ipid = f_context.argument(0);
    const auto l_result = f_context.server()->banPlayers(
        l_ipid, l_duration_seconds, l_args_str, l_moderator, QStringLiteral("Permanently."),
        [&l_args_str](int f_ban_id, const QString &f_until) {
            return l_args_str + "\nID: " + QString::number(f_ban_id) + "\nUntil: " + f_until;
        });

    if (l_result.clients_kicked == 0) {
        f_context.reply("Banned " + l_ipid + " for reason: " + l_args_str);
        return;
    }
    f_context.reply("Banned user with ipid " + l_ipid + " for reason: " + l_args_str);
    if (l_result.clients_kicked > 1)
        f_context.reply("Kicked " + QString::number(l_result.clients_kicked) + " clients with matching ipids.");
}

// The /kick door: everyone on the IPID; the act itself is the server's
// kick verb.
void cmdKick(CommandContext &f_context)
{
    QString l_target_ipid = f_context.argument(0);
    QString l_reason = f_context.argument(1);

    if (f_context.argc() > 2) {
        for (int i = 2; i < f_context.argc(); i++) {
            l_reason += " " + f_context.argument(i);
        }
    }

    const QList<akashi::ClientSession *> l_targets = f_context.server()->clientsByIpid(l_target_ipid);
    if (l_targets.isEmpty()) {
        f_context.reply("User with ipid not found!");
        return;
    }
    const QString l_moderator = f_context.server()->authType() == AuthType::ADVANCED
                                    ? f_context.moderatorName()
                                    : QStringLiteral("Moderator");
    f_context.server()->kickClients(l_targets, l_target_ipid, l_reason, l_moderator);
    f_context.reply("Kicked " + QString::number(l_targets.size()) + " client(s) with ipid " + l_target_ipid + " for reason: " + l_reason);
}

void cmdMods(CommandContext &f_context)
{
    QStringList l_entries;
    int l_online_count = 0;
    const QVector<akashi::ClientSession *> l_clients = f_context.server()->clients();
    for (akashi::ClientSession *l_client : l_clients) {
        if (l_client->isAuthenticated()) {
            l_entries << "---";
            if (f_context.server()->authType() != AuthType::SIMPLE) {
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

// The caller's permissions, asked the way the dispatch gate asks.
static std::function<bool(const QString &)> callerPermissions(CommandContext &f_context)
{
    return [&f_context](const QString &f_permission) {
        return f_context.canPerform(f_permission);
    };
}

void cmdCommands(CommandContext &f_context)
{
    QStringList l_entries;
    l_entries << "Allowed commands:";

    CommandRegistry *l_registry = f_context.server()->commandRegistry();
    for (const QString &l_name : l_registry->commandNames()) {
        auto l_spec = l_registry->spec(l_name);
        if (!l_spec)
            continue;
        if (l_registry->canUse(l_name, callerPermissions(f_context))) {
            QString l_info = "/" + l_name;
            if (!l_spec->aliases.isEmpty()) {
                l_info += " [aka: " + l_spec->aliases.join(", ") + "]";
            }
            l_entries << l_info;
        }
    }
    f_context.reply(l_entries.join("\n"));
}

void cmdHelp(CommandContext &f_context)
{
    CommandRegistry *l_registry = f_context.server()->commandRegistry();

    if (f_context.argc() == 0) {
        f_context.reply("Type /help <command> for help on a specific command, or /help all to list all commands.");
        return;
    }

    if (f_context.argc() > 1) {
        f_context.reply("Too many arguments. Please only use the command name.");
        return;
    }

    QString l_command_name = f_context.argument(0).toLower();

    auto l_check_permission = [&f_context, l_registry](const QString &f_command_name) -> bool {
        return l_registry->canUse(f_command_name, callerPermissions(f_context));
    };

    auto l_format_command = [l_registry](const QString &f_command_name) -> QString {
        auto l_spec = l_registry->spec(f_command_name);
        if (!l_spec)
            return {};
        QString l_text = "/" + l_spec->name;
        if (!l_spec->usage.isEmpty())
            l_text = l_spec->usage;
        l_text += "\n" + (l_spec->description.isEmpty() ? QString("No details available.") : l_spec->description);
        // A variant command lists each of its forms with its own gate.
        for (const CommandVariant &l_variant : l_spec->variants) {
            if (l_variant.usage.isEmpty())
                continue;
            l_text += "\n  " + l_variant.usage;
            if (!l_variant.description.isEmpty())
                l_text += " - " + l_variant.description;
            if (!l_variant.permissions.isEmpty())
                l_text += " [" + l_variant.permissions.join(", ") + "]";
        }
        return l_text;
    };

    QString l_message = "==Help==\n";

    if (l_command_name == "all") {
        QStringList l_entries;
        for (const QString &l_name : l_registry->commandNames()) {
            if (l_check_permission(l_name)) {
                l_entries.append(l_format_command(l_name));
            }
        }
        f_context.reply(l_message + l_entries.join("\n\n"));
        return;
    }

    if (!l_registry->contains(l_command_name)) {
        f_context.reply(l_message + "Unable to find the command " + l_command_name + ".");
        return;
    }

    if (!l_check_permission(l_command_name)) {
        f_context.reply(l_message + "You are not allowed to use the command " + l_command_name + ".");
        return;
    }

    f_context.reply(l_message + l_format_command(l_command_name));
}

void cmdMotd(CommandContext &f_context)
{
    f_context.reply("=== MOTD ===\r\n" + f_context.server()->serverSettings()->motd() + "\r\n=============");
}

void cmdSetMotd(CommandContext &f_context)
{
    QString l_motd = f_context.arguments().join(" ");
    f_context.server()->setMotd(l_motd);
    f_context.reply("MOTD has been changed.");
}

void cmdBans(CommandContext &f_context)
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

void cmdUnban(CommandContext &f_context)
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

void cmdAbout(CommandContext &f_context)
{
    f_context.sendPacket("CT", {"The akashi dev team", "Thank you for using akashi! Made with love by scatterflower, with help from in1tiate, Salanto, and mangosarentliterature. akashi " + QCoreApplication::applicationVersion() + ". For documentation and reporting issues, see the source: https://github.com/AttorneyOnline/akashi"});
}

bool applySanctionSchedule(CommandContext &f_context, TargetPlayer &f_target, const QString &f_sanction_id)
{
    if (f_context.argc() <= 1) {
        // A plain sanction holds until lifted by hand; an older stored
        // lift must not cut it short.
        f_context.server()->clearStoredSanction(f_target.ipid(), f_sanction_id);
        return true;
    }
    const QString l_text = QStringList(f_context.arguments().mid(1)).join(QLatin1Char(' '));
    const auto l_until = akashi::parseWhen(l_text, QDateTime::currentDateTime());
    if (!l_until.has_value()) {
        f_context.reply("Could not read \"" + l_text + "\" as a time. Use a duration like 1d12h30m, a weekday, or a date like 01.01.2028 18:00.");
        return false;
    }
    const QString l_moderator = f_context.server()->authType() == AuthType::ADVANCED
                                    ? f_context.moderatorName()
                                    : QStringLiteral("moderator");
    f_context.server()->applyTimedSanction(f_target.ipid(), f_sanction_id, *l_until, l_moderator);
    f_context.reply("The sanction lifts itself " + l_until->toString("yyyy-MM-dd hh:mm") + ".");
    return true;
}

void clearSanctionSchedule(CommandContext &f_context, TargetPlayer &f_target, const QString &f_sanction_id)
{
    f_context.server()->clearStoredSanction(f_target.ipid(), f_sanction_id);
}

void cmdMute(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        if (!applySanctionSchedule(f_context, *l_target, akashi::sanction::muted))
            return;
        if (l_target->hasSanction(akashi::sanction::muted))
            f_context.reply("That player is already muted!");
        else {
            f_context.reply("Muted player.");
            l_target->reply("You were muted by a moderator. " + reprimand(f_context.server()));
        }
        l_target->setSanction(akashi::sanction::muted, true);
    }
}

void cmdUnmute(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        clearSanctionSchedule(f_context, *l_target, akashi::sanction::muted);
        if (!l_target->hasSanction(akashi::sanction::muted))
            f_context.reply("That player is not muted!");
        else {
            f_context.reply("Unmuted player.");
            l_target->reply("You were unmuted by a moderator. " + reprimand(f_context.server(), true));
        }
        l_target->setSanction(akashi::sanction::muted, false);
    }
}

void cmdOocMute(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        if (!applySanctionSchedule(f_context, *l_target, akashi::sanction::ooc_muted))
            return;
        if (l_target->hasSanction(akashi::sanction::ooc_muted))
            f_context.reply("That player is already OOC muted!");
        else {
            f_context.reply("OOC muted player.");
            l_target->reply("You were OOC muted by a moderator. " + reprimand(f_context.server()));
        }
        l_target->setSanction(akashi::sanction::ooc_muted, true);
    }
}

void cmdOocUnmute(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        clearSanctionSchedule(f_context, *l_target, akashi::sanction::ooc_muted);
        if (!l_target->hasSanction(akashi::sanction::ooc_muted))
            f_context.reply("That player is not OOC muted!");
        else {
            f_context.reply("OOC unmuted player.");
            l_target->reply("You were OOC unmuted by a moderator. " + reprimand(f_context.server(), true));
        }
        l_target->setSanction(akashi::sanction::ooc_muted, false);
    }
}

void cmdBlockWtce(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        if (!applySanctionSchedule(f_context, *l_target, akashi::sanction::wtce_blocked))
            return;
        if (l_target->hasSanction(akashi::sanction::wtce_blocked))
            f_context.reply("That player is already judge blocked!");
        else {
            f_context.reply("Revoked player's access to judge controls.");
            l_target->reply("A moderator revoked your judge controls access. " + reprimand(f_context.server()));
        }
        l_target->setSanction(akashi::sanction::wtce_blocked, true);
    }
}

void cmdUnblockWtce(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        clearSanctionSchedule(f_context, *l_target, akashi::sanction::wtce_blocked);
        if (!l_target->hasSanction(akashi::sanction::wtce_blocked))
            f_context.reply("That player is not judge blocked!");
        else {
            f_context.reply("Restored player's access to judge controls.");
            l_target->reply("A moderator restored your judge controls access. " + reprimand(f_context.server(), true));
        }
        l_target->setSanction(akashi::sanction::wtce_blocked, false);
    }
}

void cmdAllowBlankposting(CommandContext &f_context)
{
    akashi::Area *l_area = f_context.server()->areaById(f_context.areaId());
    l_area->toggleBlankposting();
    if (l_area->isBlankpostingAllowed() == false) {
        f_context.replyToArea(f_context.name() + " has set blankposting in the area to forbidden.");
    }
    else {
        f_context.replyToArea(f_context.name() + " has set blankposting in the area to allowed.");
    }
}

void cmdBanInfo(CommandContext &f_context)
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

void cmdReload(CommandContext &f_context)
{
    f_context.server()->reloadSettings();
    f_context.reply("Reloaded configurations");
}

void cmdForceNointPres(CommandContext &f_context)
{
    akashi::Area *l_area = f_context.server()->areaById(f_context.areaId());
    l_area->toggleImmediate();
    QString l_state = l_area->forceImmediate() ? "on." : "off.";
    f_context.reply("Forced immediate text processing in this area is now " + l_state);
}

void cmdAllowIniswap(CommandContext &f_context)
{
    akashi::Area *l_area = f_context.server()->areaById(f_context.areaId());
    l_area->toggleIniswap();
    QString state = l_area->isIniswapAllowed() ? "allowed." : "disallowed.";
    f_context.reply("Iniswapping in this area is now " + state);
}

void cmdPermitSaving(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        l_target->setTestimonySaving(true);
        f_context.reply("Testimony saving has been enabled for client " + QString::number(l_target->clientId()));
    }
}

// The /kick_uid door: one client only; the act itself is the server's
// kick verb, so unlike before this door leaves a log entry like /kick.
void cmdKickUid(CommandContext &f_context)
{
    QString l_reason = f_context.argument(1);

    if (f_context.argc() > 2) {
        for (int i = 2; i < f_context.argc(); i++) {
            l_reason += " " + f_context.argument(i);
        }
    }

    if (auto l_target = f_context.resolveTarget()) {
        const QString l_moderator = f_context.server()->authType() == AuthType::ADVANCED
                                        ? f_context.moderatorName()
                                        : QStringLiteral("Moderator");
        f_context.server()->kickClients({f_context.server()->clientById(l_target->clientId())}, l_target->ipid(), l_reason, l_moderator);
        f_context.reply("Kicked client with UID " + f_context.argument(0) + " for reason: " + l_reason);
    }
}

void cmdUpdateBan(CommandContext &f_context)
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

void cmdNotice(CommandContext &f_context)
{
    sendNotice(f_context, f_context.arguments().join(" "));
}

void cmdNoticeG(CommandContext &f_context)
{
    sendNotice(f_context, f_context.arguments().join(" "), true);
}

void cmdKickOther(CommandContext &f_context)
{
    int l_kick_counter = 0;

    QList<akashi::ClientSession *> l_target_clients;
    const QList<akashi::ClientSession *> l_targets_hwid = f_context.server()->clientsByHwid(f_context.hwid());
    l_target_clients = f_context.server()->clientsByIpid(f_context.ipid());

    for (akashi::ClientSession *l_target_candidate : std::as_const(l_targets_hwid)) {
        if (!l_target_clients.contains(l_target_candidate)) {
            l_target_clients.append(l_target_candidate);
        }
    }

    QMutableListIterator<akashi::ClientSession *> it(l_target_clients);
    while (it.hasNext()) {
        if (it.next()->clientId() == f_context.clientId()) {
            it.remove();
        }
    }

    for (akashi::ClientSession *l_target_client : std::as_const(l_target_clients)) {
        l_target_client->closeSocket();
        l_kick_counter++;
    }
    f_context.reply("Kicked " + QString::number(l_kick_counter) + " multiclients from the server.");
}

void registerModerationCommands(CommandRegistry &f_registry)
{
    f_registry.registerCommand(
        {QStringLiteral("ban"), {}, {akashi::permission::ban}, 3, QStringLiteral("/ban <ipid> <duration> <reason>"), QStringLiteral("Bans a client by IPID.")},
        cmdBan, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("kick"), {}, {akashi::permission::kick}, 2, QStringLiteral("/kick <ipid> <reason>"), QStringLiteral("Kicks all clients with the given IPID.")},
        cmdKick, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("mods"), {}, {akashi::permission::user}, 0, QStringLiteral("/mods"), QStringLiteral("Lists currently logged-in moderators.")},
        cmdMods, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("commands"), {}, {akashi::permission::user}, 0, QStringLiteral("/commands"), QStringLiteral("Lists all commands available to you.")},
        cmdCommands, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("help"), {}, {akashi::permission::user}, 0, QStringLiteral("/help [command|all]"), QStringLiteral("Displays help for a command.")},
        cmdHelp, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("motd"), {}, {akashi::permission::user}, 0, QStringLiteral("/motd"), QStringLiteral("Displays the Message of the Day.")},
        cmdMotd, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("set_motd"), {QStringLiteral("setmotd")}, {akashi::permission::motd}, 1, QStringLiteral("/set_motd <message>"), QStringLiteral("Sets the Message of the Day.")},
        cmdSetMotd, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("bans"), {}, {akashi::permission::ban}, 0, QStringLiteral("/bans"), QStringLiteral("Lists the last 5 bans.")},
        cmdBans, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("unban"), {}, {akashi::permission::ban}, 1, QStringLiteral("/unban <ban_id>"), QStringLiteral("Invalidates a ban by its ID.")},
        cmdUnban, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("about"), {}, {akashi::permission::user}, 0, QStringLiteral("/about"), QStringLiteral("Displays server version info.")},
        cmdAbout, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("mute"), {}, {akashi::permission::mute}, 1, QStringLiteral("/mute <id> [until]"), QStringLiteral("Mutes a client, until a time like 1d12h, friday or 01.01.2028 18:00 if one is given.")},
        cmdMute, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("unmute"), {}, {akashi::permission::mute}, 1, QStringLiteral("/unmute <id>"), QStringLiteral("Unmutes a client.")},
        cmdUnmute, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("ooc_mute"), {QStringLiteral("mute_ooc"), QStringLiteral("oocmute")}, {akashi::permission::mute}, 1, QStringLiteral("/ooc_mute <id> [until]"), QStringLiteral("OOC-mutes a client, until a time like 1d12h, friday or 01.01.2028 18:00 if one is given.")},
        cmdOocMute, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("ooc_unmute"), {QStringLiteral("unmute_ooc"), QStringLiteral("oocunmute")}, {akashi::permission::mute}, 1, QStringLiteral("/ooc_unmute <id>"), QStringLiteral("OOC-unmutes a client.")},
        cmdOocUnmute, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("block_wtce"), {QStringLiteral("blockwtce")}, {akashi::permission::mute}, 1, QStringLiteral("/block_wtce <id> [until]"), QStringLiteral("Blocks a client from using judge controls, until a time like 1d12h if one is given.")},
        cmdBlockWtce, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("unblock_wtce"), {QStringLiteral("unblockwtce")}, {akashi::permission::mute}, 1, QStringLiteral("/unblock_wtce <id>"), QStringLiteral("Restores a client's judge controls.")},
        cmdUnblockWtce, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("allow_blankposting"), {QStringLiteral("allowblankposting")}, {akashi::permission::chat_moderator}, 0, QStringLiteral("/allow_blankposting"), QStringLiteral("Toggles blankposting in the area.")},
        cmdAllowBlankposting, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("baninfo"), {}, {akashi::permission::ban}, 1, QStringLiteral("/baninfo <id> [type]"), QStringLiteral("Looks up ban info.")},
        cmdBanInfo, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("reload"), {}, {akashi::permission::super}, 0, QStringLiteral("/reload"), QStringLiteral("Reloads server configuration.")},
        cmdReload, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("force_noint_pres"), {QStringLiteral("forceimmediate")}, {akashi::permission::gamemaster}, 0, QStringLiteral("/force_noint_pres"), QStringLiteral("Toggles forced immediate text processing.")},
        cmdForceNointPres, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("allow_iniswap"), {QStringLiteral("allowiniswap")}, {akashi::permission::gamemaster}, 0, QStringLiteral("/allow_iniswap"), QStringLiteral("Toggles iniswap permission in the area.")},
        cmdAllowIniswap, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("permitsaving"), {}, {akashi::permission::chat_moderator}, 1, QStringLiteral("/permitsaving <id>"), QStringLiteral("Grants a client permission to save testimony.")},
        cmdPermitSaving, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("kick_uid"), {QStringLiteral("kickuid")}, {akashi::permission::kick}, 2, QStringLiteral("/kick_uid <id> <reason>"), QStringLiteral("Kicks a single client by UID.")},
        cmdKickUid, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("update_ban"), {QStringLiteral("updateban")}, {akashi::permission::ban}, 3, QStringLiteral("/update_ban <ban_id> <field> <value>"), QStringLiteral("Updates a ban's duration or reason.")},
        cmdUpdateBan, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("notice"), {}, {akashi::permission::send_notice}, 1, QStringLiteral("/notice <message>"), QStringLiteral("Sends a notice to the area.")},
        cmdNotice, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("noticeg"), {}, {akashi::permission::send_notice}, 1, QStringLiteral("/noticeg <message>"), QStringLiteral("Sends a server-wide notice.")},
        cmdNoticeG, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("kick_other"), {QStringLiteral("kickother")}, {akashi::permission::user}, 0, QStringLiteral("/kick_other"), QStringLiteral("Kicks your other connected clients.")},
        cmdKickOther, QStringLiteral("core"));
}

} // namespace akashi::commands
