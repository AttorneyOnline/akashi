#include "commands/moderation_commands.h"

#include "akashi/scheduler.h"
#include "akashi/service_registry.h"
#include "core/client_session.h"
#include "core/command_context.h"
#include "core/command_registry.h"
#include "core/command_spec.h"
#include "core/db_manager.h"
#include "core/permission_registry.h"
#include "core/plugin_manager.h"
#include "core/server_context.h"
#include "core/server_settings.h"
#include "world/area.h"
#include "world/floor.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QRegularExpression>
#include <QSet>

#include <functional>
#include <optional>
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
    // Everyone may see who is around to ask for help; the where-and-as-
    // whom detail is a staff radar that lets an abuser time their
    // behavior to where staff aren't, so it stays with staff.
    const bool l_staff_radar = f_context.canPerform(akashi::permission::see_staff_presence);
    QStringList l_entries;
    int l_online_count = 0;
    const QVector<akashi::ClientSession *> l_clients = f_context.server()->clients();
    for (akashi::ClientSession *l_client : l_clients) {
        if (l_client->isAuthenticated()) {
            l_entries << "---";
            if (f_context.server()->authType() != AuthType::SIMPLE) {
                l_entries << "Moderator: " + l_client->moderatorName();
            }
            l_entries << "OOC name: " + l_client->name();
            l_entries << "ID: " + QString::number(l_client->clientId());
            if (l_staff_radar) {
                if (f_context.server()->authType() != AuthType::SIMPLE) {
                    l_entries << "Role:" << l_client->aclRoleId();
                }
                l_entries << "Area: " + QString::number(l_client->areaId());
                l_entries << "Character: " + l_client->character();
            }
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

// The areas where a command's gate resolves for this caller, capped for
// readability, with the overflow counted instead of hidden.
static QString areasWhereUsable(CommandContext &f_context, CommandRegistry *f_registry, const QString &f_name)
{
    QStringList l_open;
    int l_open_count = 0;
    const int l_area_count = f_context.server()->areaCount();
    for (int i = 0; i < l_area_count; i++) {
        if (i == f_context.areaId())
            continue;
        if (f_registry->canUse(f_name, [&f_context, i](const QString &f_permission) { return f_context.canPerformIn(f_permission, i); })) {
            l_open_count++;
            if (l_open.size() < 3)
                l_open << f_context.server()->areaName(i);
        }
    }
    if (l_open.isEmpty())
        return {};
    QString l_text = l_open.join(", ");
    if (l_open_count > l_open.size())
        l_text += ", +" + QString::number(l_open_count - l_open.size()) + " more";
    return l_text;
}

void cmdCommands(CommandContext &f_context)
{
    QStringList l_entries;
    l_entries << "Allowed commands:";

    // Verbs closed here but open in another area's offers are annotated,
    // never hidden - a command blinking away reads as "removed".
    QStringList l_elsewhere;
    CommandRegistry *l_registry = f_context.server()->commandRegistry();
    for (const QString &l_name : l_registry->commandNames()) {
        auto l_spec = l_registry->spec(l_name);
        if (!l_spec)
            continue;
        QString l_info = "/" + l_name;
        if (!l_spec->aliases.isEmpty()) {
            l_info += " [aka: " + l_spec->aliases.join(", ") + "]";
        }
        if (l_registry->canUse(l_name, callerPermissions(f_context))) {
            l_entries << l_info;
            continue;
        }
        const QString l_where = areasWhereUsable(f_context, l_registry, l_name);
        if (!l_where.isEmpty()) {
            l_elsewhere << l_info + " (" + l_where + ")";
        }
    }
    if (!l_elsewhere.isEmpty()) {
        l_entries << "Elsewhere on this server:" << l_elsewhere;
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
        // A variant command lists each of its forms with its own gate; an
        // all-of group renders its members joined by "+".
        for (const CommandVariant &l_variant : l_spec->variants) {
            if (l_variant.usage.isEmpty())
                continue;
            l_text += "\n  " + l_variant.usage;
            if (!l_variant.description.isEmpty())
                l_text += " - " + l_variant.description;
            QStringList l_gate = l_variant.permissions;
            for (const QStringList &l_group : l_variant.requirement_groups)
                l_gate << l_group.join("+");
            if (!l_gate.isEmpty())
                l_text += " [" + l_gate.join(", ") + "]";
        }
        // The declared value-dependent escalation and target immunity are
        // part of the gate, so the help shows them.
        if (!l_spec->escalates_to.isEmpty()) {
            l_text += "\nNeeds " + l_spec->escalates_to;
            if (!l_spec->escalates_when.isEmpty())
                l_text += " when " + l_spec->escalates_when;
            l_text += ".";
        }
        if (!l_spec->target_immune_if.isEmpty())
            l_text += "\nCannot target holders of " + l_spec->target_immune_if + ".";
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
        // A verb closed here may resolve in another area's offers - say
        // where instead of reading as "no such power anywhere".
        QString l_refusal = l_message + "You are not allowed to use the command " + l_command_name + ".";
        const QString l_where = areasWhereUsable(f_context, l_registry, l_command_name);
        if (!l_where.isEmpty())
            l_refusal += " It works in: " + l_where + ".";
        f_context.reply(l_refusal);
        return;
    }

    f_context.reply(l_message + l_format_command(l_command_name));
}

// One grant match phrased for a player. A personal grant is reported as
// existing, never by the identity behind it - /why must not become
// alt-linkage reconnaissance.
static QString describeGrantMatch(CommandContext &f_context, const akashi::GrantMatch &f_match)
{
    const akashi::Grant &l_grant = f_match.grant;
    const auto l_audience_words = [&l_grant]() -> QString {
        switch (l_grant.audience.kind) {
        case akashi::AudienceKind::Everyone:
            return QStringLiteral("everyone");
        case akashi::AudienceKind::Participants:
            return QStringLiteral("participants");
        case akashi::AudienceKind::Role:
            return QStringLiteral("role \"") + l_grant.audience.role_id + QStringLiteral("\"");
        case akashi::AudienceKind::Person:
            return QStringLiteral("a personal grant");
        }
        return {};
    };
    if (f_match.source_id == QStringLiteral("role_grants")) {
        const bool l_super = f_context.server()->aclRolesHandler()->roleById(l_grant.audience.role_id).canPerform(akashi::permission::super);
        return "the role \"" + l_grant.audience.role_id + "\"" + (l_super ? QStringLiteral(" (holds every permission)") : QString()) + " at server scope";
    }
    if (f_match.source_id == QStringLiteral("simple_auth"))
        return QStringLiteral("being logged in - simple auth grants everything");
    if (f_match.source_id == QStringLiteral("area_owner"))
        return QStringLiteral("being CM in this area");
    if (f_match.source_id == QStringLiteral("place_offers")) {
        const QString l_place = l_grant.scope == akashi::GrantScope::Floor ? QStringLiteral("this floor's offer") : QStringLiteral("this area's offer");
        return l_place + " to " + l_audience_words() + " (" + l_grant.owner + ")";
    }
    return "a server-scope grant to " + l_audience_words() + " (" + l_grant.owner + ")";
}

// The verdict line for one permission: the decisive fact first - a mask
// outranks any grant - and, when refused here, where it does resolve.
static QString whyLine(CommandContext &f_context, akashi::ClientSession *f_subject, const QString &f_permission)
{
    const akashi::Resolution l_resolution = f_subject->explainPermission(f_permission);
    if (!l_resolution.masked_by.isEmpty()) {
        return "[x] " + f_permission + " - blocked by the \"" + l_resolution.masked_by.join("\", \"") + "\" sanction; a sanction overrides every grant.";
    }
    if (!l_resolution.matched.isEmpty()) {
        return "[+] " + f_permission + " - granted by " + describeGrantMatch(f_context, l_resolution.matched.first()) + ".";
    }
    QString l_line = "[x] " + f_permission + " - no role grant and nothing offers it here.";
    QStringList l_open;
    int l_open_count = 0;
    const int l_area_count = f_context.server()->areaCount();
    for (int i = 0; i < l_area_count; i++) {
        if (i == f_subject->areaId())
            continue;
        if (f_subject->canPerformIn(f_permission, i)) {
            l_open_count++;
            if (l_open.size() < 3)
                l_open << f_context.server()->areaName(i);
        }
    }
    if (!l_open.isEmpty()) {
        l_line += " It works in: " + l_open.join(", ");
        if (l_open_count > l_open.size())
            l_line += ", +" + QString::number(l_open_count - l_open.size()) + " more";
        l_line += ".";
    }
    return l_line;
}

// Renders one form's whole gate the way /help does.
static QString gateWords(const QStringList &f_permissions, const QList<QStringList> &f_groups)
{
    QStringList l_gate = f_permissions;
    for (const QStringList &l_group : f_groups)
        l_gate << l_group.join("+");
    return l_gate.isEmpty() ? QStringLiteral("nothing") : l_gate.join(", ");
}

void cmdWhy(CommandContext &f_context)
{
    // The two-argument form asks about another player.
    akashi::ClientSession *l_subject = f_context.server()->clientById(f_context.clientId());
    QString l_query = f_context.argument(0);
    if (f_context.argc() == 2) {
        const auto l_target_id = f_context.argumentAsInt(0);
        akashi::ClientSession *l_target = l_target_id ? f_context.server()->clientById(*l_target_id) : nullptr;
        if (!l_target) {
            f_context.reply("No client with that ID found.");
            return;
        }
        l_subject = l_target;
        l_query = f_context.argument(1);
    }
    if (!l_subject) {
        return;
    }
    l_query = l_query.toLower();
    if (l_query.startsWith(QLatin1Char('/')))
        l_query.remove(0, 1);

    QStringList l_lines;
    CommandRegistry *l_registry = f_context.server()->commandRegistry();
    if (l_registry->contains(l_query)) {
        // Players think in commands: resolve the binding, explain every
        // permission the gate can ask for, and surface the declared
        // escalation and immunity - the parts outside canPerform.
        const auto l_spec = l_registry->spec(l_query);
        QStringList l_permissions;
        const auto l_collect = [&l_permissions](const QStringList &f_flat, const QList<QStringList> &f_groups) {
            for (const QString &l_permission : f_flat)
                if (!l_permissions.contains(l_permission))
                    l_permissions.append(l_permission);
            for (const QStringList &l_group : f_groups)
                for (const QString &l_permission : l_group)
                    if (!l_permissions.contains(l_permission))
                        l_permissions.append(l_permission);
        };
        if (l_spec->variants.isEmpty()) {
            l_lines << "/" + l_spec->name + " needs: " + gateWords(l_spec->permissions, l_spec->requirement_groups);
            l_collect(l_spec->permissions, l_spec->requirement_groups);
        }
        else {
            for (const CommandVariant &l_variant : l_spec->variants) {
                l_lines << l_variant.usage + " needs: " + gateWords(l_variant.permissions, l_variant.requirement_groups);
                l_collect(l_variant.permissions, l_variant.requirement_groups);
            }
        }
        if (!l_spec->escalates_to.isEmpty()) {
            l_lines << "Also needs " + l_spec->escalates_to + (l_spec->escalates_when.isEmpty() ? QString() : " when " + l_spec->escalates_when) + ".";
            if (!l_permissions.contains(l_spec->escalates_to))
                l_permissions.append(l_spec->escalates_to);
        }
        if (!l_spec->target_immune_if.isEmpty())
            l_lines << "Cannot target holders of " + l_spec->target_immune_if + ".";
        for (const QString &l_permission : std::as_const(l_permissions))
            l_lines << whyLine(f_context, l_subject, l_permission);
    }
    else if (f_context.server()->permissionRegistry()->isRegistered(l_query)) {
        l_lines << whyLine(f_context, l_subject, l_query);
    }
    else {
        f_context.reply("There is no command or permission named \"" + l_query + "\".");
        return;
    }
    f_context.reply(l_lines.join("\n"));
}

// One stored grant as an operator row; behind see_ipids, so person keys
// print in the clear like the rest of the moderation surface.
static QString grantRow(const akashi::Grant &f_grant)
{
    QString l_audience;
    switch (f_grant.audience.kind) {
    case akashi::AudienceKind::Everyone:
        l_audience = QStringLiteral("everyone");
        break;
    case akashi::AudienceKind::Participants:
        l_audience = QStringLiteral("participants");
        break;
    case akashi::AudienceKind::Role:
        l_audience = QStringLiteral("role ") + f_grant.audience.role_id;
        break;
    case akashi::AudienceKind::Person:
        l_audience = QStringLiteral("person ") + f_grant.audience.person_key;
        break;
    }
    return "  " + f_grant.permission + " -> " + l_audience + " [" + f_grant.owner + "]";
}

void cmdDumpGrants(CommandContext &f_context)
{
    QStringList l_lines;
    l_lines << "== Stored grants ==";
    l_lines << "Server:";
    const QList<akashi::Grant> l_server_grants = f_context.server()->permissionRegistry()->serverGrants();
    for (const akashi::Grant &l_grant : l_server_grants)
        l_lines << grantRow(l_grant);
    QSet<int> l_seen_floors;
    const int l_area_count = f_context.server()->areaCount();
    for (int i = 0; i < l_area_count; i++) {
        akashi::Area *l_area = f_context.server()->areaById(i);
        const int l_floor_id = l_area->floorId();
        if (!l_seen_floors.contains(l_floor_id)) {
            l_seen_floors.insert(l_floor_id);
            if (const akashi::Floor *l_floor = f_context.server()->floorById(l_floor_id); l_floor && !l_floor->grants.isEmpty()) {
                l_lines << "Floor " + l_floor->name + ":";
                for (const akashi::Grant &l_grant : l_floor->grants)
                    l_lines << grantRow(l_grant);
            }
        }
        if (!l_area->grants().isEmpty()) {
            l_lines << "Area " + l_area->name() + ":";
            for (const akashi::Grant &l_grant : l_area->grants())
                l_lines << grantRow(l_grant);
        }
    }
    l_lines << "Live offers (ownership, lock state, simple auth) resolve per query and are not listed.";
    f_context.reply(l_lines.join("\n"));
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

void cmdLiftSanction(CommandContext &f_context)
{
    const QString l_ipid = f_context.argument(0);
    const QString l_sanction = f_context.argument(1).toLower();
    if (!f_context.server()->databaseManager()->sanctionRow(l_ipid, l_sanction).has_value()) {
        f_context.reply("No stored \"" + l_sanction + "\" sanction for that IPID.");
        return;
    }
    f_context.server()->removeSanction(l_ipid, l_sanction);
    f_context.reply("Lifted \"" + l_sanction + "\" from " + l_ipid + ".");
}

void cmdAbout(CommandContext &f_context)
{
    const QString l_akashi_about = "Thank you for using akashi! Made with love by scatterflower, with help from in1tiate, Salanto, and mangosarentliterature. akashi " + QCoreApplication::applicationVersion() + ". For documentation and reporting issues, see the source: https://github.com/AttorneyOnline/akashi";
    auto l_plugins = f_context.services()->resolve<PluginManager>(QStringLiteral("akashi.plugins"));
    const QMap<QString, QString> l_abouts = l_plugins ? l_plugins->abouts() : QMap<QString, QString>();

    if (f_context.argc() >= 1) {
        const QString l_name = f_context.argument(0);
        if (l_name.compare(QStringLiteral("akashi"), Qt::CaseInsensitive) == 0)
            f_context.sendPacket("CT", {"The akashi dev team", l_akashi_about});
        else if (l_abouts.contains(l_name))
            f_context.sendPacket("CT", {l_name, l_abouts.value(l_name)});
        else {
            QStringList l_names(QStringLiteral("akashi"));
            l_names += l_abouts.keys();
            f_context.reply("Nothing is registered under \"" + l_name + "\". Try one of: " + l_names.join(", ") + ".");
        }
        return;
    }

    QStringList l_sections(l_akashi_about);
    for (auto it = l_abouts.cbegin(); it != l_abouts.cend(); ++it)
        l_sections << it.key() + ": " + it.value();
    f_context.sendPacket("CT", {"The akashi dev team", l_sections.join("\n\n")});
}

bool applySanction(CommandContext &f_context, TargetPlayer &f_target, const QString &f_sanction_id, const QString &f_data)
{
    // Staff protection lives at administration time, the one spot that
    // covers block sanctions, text transforms and charcurse alike; only
    // super reaches past it.
    if (f_target.canPerform(akashi::permission::sanction_immune) && !f_context.canPerform(akashi::permission::super)) {
        f_context.reply("That player is protected from sanctions.");
        return false;
    }
    const QString l_moderator = f_context.server()->authType() == AuthType::ADVANCED
                                    ? f_context.moderatorName()
                                    : QStringLiteral("moderator");
    std::optional<QDateTime> l_until = std::nullopt;
    if (f_context.argc() > 1) {
        const QString l_text = QStringList(f_context.arguments().mid(1)).join(QLatin1Char(' '));
        l_until = akashi::parseWhen(l_text, QDateTime::currentDateTime());
        if (!l_until.has_value()) {
            f_context.reply("Could not read \"" + l_text + "\" as a time. Use a duration like 1d12h30m, a weekday, or a date like 01.01.2028 18:00.");
            return false;
        }
    }
    // The issuer never sweeps themselves in by sharing an IP or machine
    // with the target; targeting their own id on purpose still works.
    akashi::ClientSession *l_exempt = nullptr;
    bool l_id_ok = false;
    const int l_target_id = f_context.argument(0).toInt(&l_id_ok);
    if (l_id_ok && l_target_id != f_context.clientId()) {
        l_exempt = f_context.server()->clientById(f_context.clientId());
    }
    f_context.server()->applySanction(f_target.ipid(), f_target.hwid(), f_sanction_id, l_moderator, l_until, f_data, l_exempt);
    if (l_until.has_value()) {
        f_context.reply("The sanction lifts itself " + l_until->toString("yyyy-MM-dd hh:mm") + ".");
    }
    return true;
}

void liftSanction(CommandContext &f_context, TargetPlayer &f_target, const QString &f_sanction_id)
{
    f_context.server()->removeSanction(f_target.ipid(), f_sanction_id);
}

void cmdMute(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        const bool l_was_sanctioned = l_target->hasSanction(akashi::sanction::muted);
        if (!applySanction(f_context, *l_target, akashi::sanction::muted))
            return;
        if (l_was_sanctioned)
            f_context.reply("That player is already muted!");
        else {
            f_context.reply("Muted player.");
            l_target->reply("You were muted by a moderator. " + reprimand(f_context.server()));
        }
    }
}

void cmdUnmute(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        const bool l_was_sanctioned = l_target->hasSanction(akashi::sanction::muted);
        liftSanction(f_context, *l_target, akashi::sanction::muted);
        if (!l_was_sanctioned)
            f_context.reply("That player is not muted!");
        else {
            f_context.reply("Unmuted player.");
            l_target->reply("You were unmuted by a moderator. " + reprimand(f_context.server(), true));
        }
    }
}

void cmdOocMute(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        const bool l_was_sanctioned = l_target->hasSanction(akashi::sanction::ooc_muted);
        if (!applySanction(f_context, *l_target, akashi::sanction::ooc_muted))
            return;
        if (l_was_sanctioned)
            f_context.reply("That player is already OOC muted!");
        else {
            f_context.reply("OOC muted player.");
            l_target->reply("You were OOC muted by a moderator. " + reprimand(f_context.server()));
        }
    }
}

void cmdOocUnmute(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        const bool l_was_sanctioned = l_target->hasSanction(akashi::sanction::ooc_muted);
        liftSanction(f_context, *l_target, akashi::sanction::ooc_muted);
        if (!l_was_sanctioned)
            f_context.reply("That player is not OOC muted!");
        else {
            f_context.reply("OOC unmuted player.");
            l_target->reply("You were OOC unmuted by a moderator. " + reprimand(f_context.server(), true));
        }
    }
}

void cmdBlockWtce(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        const bool l_was_sanctioned = l_target->hasSanction(akashi::sanction::wtce_blocked);
        if (!applySanction(f_context, *l_target, akashi::sanction::wtce_blocked))
            return;
        if (l_was_sanctioned)
            f_context.reply("That player is already judge blocked!");
        else {
            f_context.reply("Revoked player's access to judge controls.");
            l_target->reply("A moderator revoked your judge controls access. " + reprimand(f_context.server()));
        }
    }
}

void cmdUnblockWtce(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        const bool l_was_sanctioned = l_target->hasSanction(akashi::sanction::wtce_blocked);
        liftSanction(f_context, *l_target, akashi::sanction::wtce_blocked);
        if (!l_was_sanctioned)
            f_context.reply("That player is not judge blocked!");
        else {
            f_context.reply("Restored player's access to judge controls.");
            l_target->reply("A moderator restored your judge controls access. " + reprimand(f_context.server(), true));
        }
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
    // The old per-session flag is a real person-scope grant now: it
    // covers every window the person owns and reads back through the
    // same resolution as any other permission.
    if (auto l_target = f_context.resolveTarget()) {
        f_context.server()->permissionRegistry()->addGrant(
            {akashi::permission::save_testimony, akashi::Audience::person(l_target->ipid()), akashi::GrantScope::Server, QStringLiteral("command")});
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
        {QStringLiteral("mods"), {}, {akashi::permission::info_mods}, 0, QStringLiteral("/mods"), QStringLiteral("Lists currently logged-in moderators.")},
        cmdMods, QStringLiteral("core"));

    {
        // /why mirrors the resolution pipeline: the self form is open to
        // every player, the target form is staff visibility.
        CommandSpec l_why;
        l_why.name = QStringLiteral("why");
        l_why.usage = QStringLiteral("/why [id] <permission|command>");
        l_why.description = QStringLiteral("Explains why you (or another player) can or cannot do something here.");
        l_why.variants = {
            {.id = QStringLiteral("self"), .min_args = 1, .max_args = 1, .permissions = {akashi::permission::info_why}, .usage = QStringLiteral("/why <permission|command>"), .description = QStringLiteral("Asks about yourself."), .handler = cmdWhy},
            {.id = QStringLiteral("target"), .min_args = 2, .max_args = 2, .permissions = {akashi::permission::see_staff_presence}, .usage = QStringLiteral("/why <id> <permission|command>"), .description = QStringLiteral("Asks about another player."), .handler = cmdWhy},
        };
        f_registry.registerCommand(l_why, QStringLiteral("core"));
    }

    f_registry.registerCommand(
        {QStringLiteral("dumpgrants"), {QStringLiteral("dump_grants")}, {akashi::permission::see_ipids}, 0, QStringLiteral("/dumpgrants"), QStringLiteral("Lists every stored grant with its audience and owner tag.")},
        cmdDumpGrants, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("commands"), {}, {akashi::permission::info_commands}, 0, QStringLiteral("/commands"), QStringLiteral("Lists all commands available to you.")},
        cmdCommands, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("help"), {}, {akashi::permission::info_help}, 0, QStringLiteral("/help [command|all]"), QStringLiteral("Displays help for a command.")},
        cmdHelp, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("motd"), {}, {akashi::permission::info_motd}, 0, QStringLiteral("/motd"), QStringLiteral("Displays the Message of the Day.")},
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
        {QStringLiteral("lift_sanction"), {QStringLiteral("liftsanction")}, {akashi::permission::sanction_mute, akashi::permission::sanction_ooc_mute, akashi::permission::sanction_block_dj, akashi::permission::sanction_block_wtce, akashi::permission::sanction_gimp, akashi::permission::sanction_disemvowel, akashi::permission::sanction_shake, akashi::permission::sanction_medieval, akashi::permission::sanction_charcurse}, 2, QStringLiteral("/lift_sanction <ipid> <sanction>"), QStringLiteral("Lifts a stored sanction by IPID, whether the person is online or not.")},
        cmdLiftSanction, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("about"), {}, {akashi::permission::info_about}, 0, QStringLiteral("/about [plugin]"), QStringLiteral("Shows who made the server and its plugins; a plugin id shows that one alone.")},
        cmdAbout, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("mute"), {}, {akashi::permission::sanction_mute}, 1, QStringLiteral("/mute <id> [until]"), QStringLiteral("Mutes a client, until a time like 1d12h, friday or 01.01.2028 18:00 if one is given.")},
        cmdMute, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("unmute"), {}, {akashi::permission::sanction_mute}, 1, QStringLiteral("/unmute <id>"), QStringLiteral("Unmutes a client.")},
        cmdUnmute, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("ooc_mute"), {QStringLiteral("mute_ooc"), QStringLiteral("oocmute")}, {akashi::permission::sanction_ooc_mute}, 1, QStringLiteral("/ooc_mute <id> [until]"), QStringLiteral("OOC-mutes a client, until a time like 1d12h, friday or 01.01.2028 18:00 if one is given.")},
        cmdOocMute, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("ooc_unmute"), {QStringLiteral("unmute_ooc"), QStringLiteral("oocunmute")}, {akashi::permission::sanction_ooc_mute}, 1, QStringLiteral("/ooc_unmute <id>"), QStringLiteral("OOC-unmutes a client.")},
        cmdOocUnmute, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("block_wtce"), {QStringLiteral("blockwtce")}, {akashi::permission::sanction_block_wtce}, 1, QStringLiteral("/block_wtce <id> [until]"), QStringLiteral("Blocks a client from using judge controls, until a time like 1d12h if one is given.")},
        cmdBlockWtce, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("unblock_wtce"), {QStringLiteral("unblockwtce")}, {akashi::permission::sanction_block_wtce}, 1, QStringLiteral("/unblock_wtce <id>"), QStringLiteral("Restores a client's judge controls.")},
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
        {QStringLiteral("force_noint_pres"), {QStringLiteral("forceimmediate")}, {akashi::permission::cm_force_noint_pres}, 0, QStringLiteral("/force_noint_pres"), QStringLiteral("Toggles forced immediate text processing.")},
        cmdForceNointPres, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("allow_iniswap"), {QStringLiteral("allowiniswap")}, {akashi::permission::cm_allow_iniswap}, 0, QStringLiteral("/allow_iniswap"), QStringLiteral("Toggles iniswap permission in the area.")},
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
