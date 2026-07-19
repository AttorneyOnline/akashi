#include "modbot_plugin.h"

#include "akashi/area_rule.h"
#include "akashi/config_store.h"
#include "akashi/database_service.h"
#include "akashi/event.h"
#include "akashi/logging_categories.h"
#include "akashi/moderation.h"
#include "akashi/permissions.h"
#include "akashi/sanctions.h"
#include "akashi/service_registry.h"
#include "core/command_context.h"
#include "core/log_service.h"
#include "core/player_directory.h"
#include "core/text_filter_registry.h"
#include "modbot_worker.h"
#include "world/rule_registry.h"

#include <QDateTime>
#include <QSqlDatabase>

#include <functional>
#include <utility>

using akashi::modbot::Event;
using akashi::modbot::Verdict;

QString ModbotPlugin::id() const { return QStringLiteral("akashi.modbot"); }
akashi::ServiceVersion ModbotPlugin::pluginVersion() const { return {1, 0, 0}; }

bool ModbotPlugin::load(akashi::ServiceRegistry &services)
{
    auto l_config = services.resolve<akashi::ConfigStore>(QStringLiteral("akashi.config"));
    auto l_filters = services.resolve<akashi::TextFilterRegistry>(QStringLiteral("akashi.textfilters"));
    auto l_rules = services.resolve<akashi::RuleRegistry>(QStringLiteral("akashi.rules"));
    auto l_databases = services.resolve<akashi::DatabaseService>(QStringLiteral("akashi.database"));
    m_players = services.resolve<akashi::PlayerDirectory>(QStringLiteral("akashi.players"));
    m_moderation = services.resolve<akashi::ModerationService>(QStringLiteral("akashi.moderation"));
    m_log = services.resolve<akashi::LogService>(QStringLiteral("akashi.log"));

    if (!l_config || !l_filters || !l_rules || !l_databases || !m_players || !m_moderation) {
        qCWarning(akashiPlugins) << "modbot: required services not available";
        return false;
    }

    const QString l_cfg = QStringLiteral("plugins/") + id();
    l_config->declarePlugin(id(), {
                                      akashi::ConfigEntry(QStringLiteral("enabled"), false,
                                                          QStringLiteral("Whether the automated moderator runs at all.")),
                                      akashi::ConfigEntry(QStringLiteral("screen_patterns"), QString(),
                                                          QStringLiteral("Comma-separated regular expressions; a message matching one never reaches other players.")),
                                      akashi::ConfigEntry(QStringLiteral("flood_messages"), 8,
                                                          QStringLiteral("Messages inside the flood window that count as a flood; 0 turns the rule off."), akashi::atLeast(0)),
                                      akashi::ConfigEntry(QStringLiteral("flood_seconds"), 5,
                                                          QStringLiteral("The flood window, in seconds."), akashi::atLeast(1)),
                                      akashi::ConfigEntry(QStringLiteral("repeat_messages"), 4,
                                                          QStringLiteral("Saying the same line this many times in a row counts as spam; 0 turns the rule off."), akashi::atLeast(0)),
                                      akashi::ConfigEntry(QStringLiteral("strike_screen_drops"), 3,
                                                          QStringLiteral("Screened messages inside the strike window that earn a verdict; 0 turns the rule off."), akashi::atLeast(0)),
                                      akashi::ConfigEntry(QStringLiteral("strike_window_seconds"), 60,
                                                          QStringLiteral("The screen-strike window, in seconds."), akashi::atLeast(1)),
                                      akashi::ConfigEntry(QStringLiteral("modcall_flood_count"), 3,
                                                          QStringLiteral("Modcalls inside the window that earn a warning; 0 turns the rule off."), akashi::atLeast(0)),
                                      akashi::ConfigEntry(QStringLiteral("modcall_flood_seconds"), 30,
                                                          QStringLiteral("The modcall window, in seconds."), akashi::atLeast(1)),
                                      akashi::ConfigEntry(QStringLiteral("mute_minutes"), 5,
                                                          QStringLiteral("The first mute length, in minutes."), akashi::atLeast(1)),
                                      akashi::ConfigEntry(QStringLiteral("mute_minutes_repeat"), 30,
                                                          QStringLiteral("The repeat-offender mute length, in minutes."), akashi::atLeast(1)),
                                      akashi::ConfigEntry(QStringLiteral("kick_after_incidents"), 4,
                                                          QStringLiteral("The incident count at which a verdict becomes a kick."), akashi::atLeast(1)),
                                      akashi::ConfigEntry(QStringLiteral("acl_role"), QStringLiteral("modbot"),
                                                          QStringLiteral("The ACL role whose permissions gate what the bot may do; grant it mute and kick like a human moderator.")),
                                      akashi::ConfigEntry(QStringLiteral("notify_moderators"), true,
                                                          QStringLiteral("Whether logged-in moderators are told about the bot's actions.")),
                                      akashi::ConfigEntry(QStringLiteral("exempt_authenticated"), true,
                                                          QStringLiteral("Whether logged-in moderators are exempt from the bot entirely.")),
                                  });

    if (!l_config->get<bool>(l_cfg, QStringLiteral("enabled"))) {
        qCInfo(akashiPlugins) << "modbot: disabled by configuration";
        return true;
    }

    const QStringList l_patterns = l_config->get<QString>(l_cfg, QStringLiteral("screen_patterns")).split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &l_pattern : l_patterns) {
        const QRegularExpression l_re(l_pattern.trimmed(), QRegularExpression::CaseInsensitiveOption);
        if (!l_re.isValid()) {
            qCWarning(akashiPlugins) << "modbot: ignoring invalid screen pattern:" << l_pattern.trimmed();
            continue;
        }
        m_rules_config.screen_patterns.append(l_re);
    }
    m_rules_config.flood_messages = l_config->get<int>(l_cfg, QStringLiteral("flood_messages"));
    m_rules_config.flood_seconds = l_config->get<int>(l_cfg, QStringLiteral("flood_seconds"));
    m_rules_config.repeat_messages = l_config->get<int>(l_cfg, QStringLiteral("repeat_messages"));
    m_rules_config.strike_screen_drops = l_config->get<int>(l_cfg, QStringLiteral("strike_screen_drops"));
    m_rules_config.strike_window_seconds = l_config->get<int>(l_cfg, QStringLiteral("strike_window_seconds"));
    m_rules_config.modcall_flood_count = l_config->get<int>(l_cfg, QStringLiteral("modcall_flood_count"));
    m_rules_config.modcall_flood_seconds = l_config->get<int>(l_cfg, QStringLiteral("modcall_flood_seconds"));
    m_rules_config.mute_minutes = l_config->get<int>(l_cfg, QStringLiteral("mute_minutes"));
    m_rules_config.mute_minutes_repeat = l_config->get<int>(l_cfg, QStringLiteral("mute_minutes_repeat"));
    m_rules_config.kick_after_incidents = l_config->get<int>(l_cfg, QStringLiteral("kick_after_incidents"));
    m_acl_role = l_config->get<QString>(l_cfg, QStringLiteral("acl_role"));
    m_notify_moderators = l_config->get<bool>(l_cfg, QStringLiteral("notify_moderators"));
    m_exempt_authenticated = l_config->get<bool>(l_cfg, QStringLiteral("exempt_authenticated"));

    if (m_log) {
        m_log->registerTemplate(QStringLiteral("modbot"), QStringLiteral("[{timestamp}][MODBOT][{target_ipid}]: {message}"));
    }

    // The pre-echo screen runs before every other filter, so screened
    // content never reaches the word filter, let alone another player.
    l_filters->registerFilter(QStringLiteral("modbot-screen"), 10,
                              akashi::ContextTextFilterFn(std::bind_front(&ModbotPlugin::screenMessage, this)),
                              true, id(), {akashi::TextChannel::Ic, akashi::TextChannel::Ooc});

    // Everything else is analysis: committed facts stream to the worker.
    l_rules->registerObserver(akashi::ICMessageEvent::id, 0, std::bind_front(&ModbotPlugin::observeChatMessage, this, Event::Kind::IcMessage), id());
    l_rules->registerObserver(akashi::OOCMessageEvent::id, 0, std::bind_front(&ModbotPlugin::observeChatMessage, this, Event::Kind::OocMessage), id());
    // Global chat, adverts, PMs and CM area-broadcasts leave the sender's
    // area outside the ooc event; count them toward the same flood window.
    l_rules->registerObserver(akashi::GlobalMessageEvent::id, 0, std::bind_front(&ModbotPlugin::observeChatMessage, this, Event::Kind::OocMessage), id());
    l_rules->registerObserver(akashi::ModcallEvent::id, 0, std::bind_front(&ModbotPlugin::observeModcall, this), id());
    l_rules->registerObserver(akashi::BanIssuedEvent::id, 0, std::bind_front(&ModbotPlugin::observeModeration, this, Event::Kind::BanIssued), id());
    l_rules->registerObserver(akashi::KickIssuedEvent::id, 0, std::bind_front(&ModbotPlugin::observeModeration, this, Event::Kind::KickIssued), id());

    // The incident store is the bot's own file; the worker opens its own
    // connection to it, since a Qt SQL connection must stay on one thread.
    const QSqlDatabase l_database = l_databases->pluginDatabase(id());
    qRegisterMetaType<Verdict>();
    m_worker = std::make_unique<akashi::modbot::Worker>(m_rules_config, l_database.databaseName());
    connect(m_worker.get(), &akashi::modbot::Worker::verdictReady, this, &ModbotPlugin::onVerdict);
    m_worker->start();

    qCInfo(akashiPlugins) << "modbot: watching with" << m_rules_config.screen_patterns.size()
                          << "screen pattern(s), acting as role" << m_acl_role;
    return true;
}

void ModbotPlugin::shutdown(akashi::ServiceRegistry &services)
{
    if (m_worker) {
        m_worker->stop();
        m_worker.reset();
    }
    if (auto l_filters = services.resolve<akashi::TextFilterRegistry>(QStringLiteral("akashi.textfilters"))) {
        l_filters->unregisterAll(id());
    }
    if (auto l_rules = services.resolve<akashi::RuleRegistry>(QStringLiteral("akashi.rules"))) {
        l_rules->unregisterObservers(id());
    }
    m_players.reset();
    m_moderation.reset();
    m_log.reset();
}

bool ModbotPlugin::isExempt(int f_client_session_id) const
{
    if (!m_exempt_authenticated || f_client_session_id < 0 || !m_players) {
        return false;
    }
    akashi::ClientSession *l_client = m_players->clientById(f_client_session_id);
    if (!l_client) {
        return false;
    }
    return akashi::TargetPlayer(l_client).isAuthenticated();
}

std::optional<QString> ModbotPlugin::screenMessage(const QString &f_text, const akashi::TextFilterContext &f_context)
{
    if (m_rules_config.screen_patterns.isEmpty() || isExempt(f_context.client_session_id)) {
        return f_text;
    }
    for (const QRegularExpression &l_pattern : std::as_const(m_rules_config.screen_patterns)) {
        if (l_pattern.match(f_text).hasMatch()) {
            Event l_event;
            l_event.kind = Event::Kind::ScreenDrop;
            l_event.epoch = QDateTime::currentSecsSinceEpoch();
            l_event.client_session_id = f_context.client_session_id;
            l_event.ipid = f_context.ipid;
            l_event.message = f_text;
            enqueue(l_event);
            return std::nullopt;
        }
    }
    return f_text;
}

void ModbotPlugin::observeChatMessage(Event::Kind f_kind, const akashi::RuleContext &f_context)
{
    if (isExempt(f_context.client_session_id)) {
        return;
    }
    Event l_event;
    l_event.kind = f_kind;
    l_event.epoch = QDateTime::currentSecsSinceEpoch();
    l_event.client_session_id = f_context.client_session_id;
    l_event.character = f_context.payload.value(QStringLiteral("char_name")).toString();
    l_event.ooc_name = f_context.payload.value(QStringLiteral("ooc_name")).toString();
    l_event.message = f_context.payload.value(QStringLiteral("message")).toString();
    // The payload carries no ipid; the main thread resolves it here, at
    // enqueue time, so the worker never has to touch the directory.
    if (m_players) {
        if (akashi::ClientSession *l_client = m_players->clientById(f_context.client_session_id)) {
            l_event.ipid = akashi::TargetPlayer(l_client).ipid();
        }
    }
    enqueue(l_event);
}

void ModbotPlugin::observeModcall(const akashi::RuleContext &f_context)
{
    if (isExempt(f_context.client_session_id)) {
        return;
    }
    Event l_event;
    l_event.kind = Event::Kind::Modcall;
    l_event.epoch = QDateTime::currentSecsSinceEpoch();
    l_event.client_session_id = f_context.client_session_id;
    l_event.ipid = f_context.payload.value(QStringLiteral("ipid")).toString();
    l_event.character = f_context.payload.value(QStringLiteral("char_name")).toString();
    l_event.ooc_name = f_context.payload.value(QStringLiteral("ooc_name")).toString();
    l_event.area_name = f_context.payload.value(QStringLiteral("area_name")).toString();
    l_event.message = f_context.payload.value(QStringLiteral("reason")).toString();
    enqueue(l_event);
}

void ModbotPlugin::observeModeration(Event::Kind f_kind, const akashi::RuleContext &f_context)
{
    Event l_event;
    l_event.kind = f_kind;
    l_event.epoch = QDateTime::currentSecsSinceEpoch();
    l_event.ipid = f_context.payload.value(QStringLiteral("target_ipid")).toString();
    l_event.message = f_context.payload.value(QStringLiteral("reason")).toString();
    enqueue(l_event);
}

void ModbotPlugin::enqueue(Event f_event)
{
    if (!m_worker) {
        return;
    }
    if (!m_worker->queue().enqueue(f_event)) {
        // Overload costs the bot context, never costs players latency -
        // and one line announces the outage instead of one per drop.
        if (m_worker->queue().droppedCount() == 1) {
            qCWarning(akashiPlugins) << "modbot: analysis queue full, dropping oldest events";
        }
    }
}

void ModbotPlugin::notifyModerators(const QString &f_text)
{
    if (!m_notify_moderators || !m_players) {
        return;
    }
    const QVector<akashi::ClientSession *> l_clients = m_players->clients();
    for (akashi::ClientSession *l_client : l_clients) {
        akashi::TargetPlayer l_player(l_client);
        if (l_player.isAuthenticated()) {
            l_player.reply(QStringLiteral("[modbot] ") + f_text);
        }
    }
}

void ModbotPlugin::warnPlayer(const Verdict &f_verdict, const QString &f_text)
{
    if (!m_players) {
        return;
    }
    if (akashi::ClientSession *l_client = m_players->clientById(f_verdict.client_session_id)) {
        akashi::TargetPlayer l_player(l_client);
        if (l_player.ipid() == f_verdict.ipid) {
            l_player.reply(f_text);
            return;
        }
    }
    // The session is gone or renumbered; reach whoever holds the ipid now.
    const QVector<akashi::ClientSession *> l_clients = m_players->clients();
    for (akashi::ClientSession *l_client : l_clients) {
        akashi::TargetPlayer l_player(l_client);
        if (l_player.ipid() == f_verdict.ipid) {
            l_player.reply(f_text);
        }
    }
}

void ModbotPlugin::logAction(const QString &f_ipid, const QString &f_text)
{
    if (m_log) {
        m_log->log({.type = QStringLiteral("modbot"),
                    .message = f_text,
                    .moderator = QStringLiteral("modbot"),
                    .target_ipid = f_ipid});
    }
}

void ModbotPlugin::onVerdict(const Verdict &f_verdict)
{
    // The same gate a human moderator answers to: the bot only holds the
    // powers its configured role grants, and anything more falls back to
    // the strongest permitted response.
    Verdict::Action l_action = f_verdict.action;
    if (l_action == Verdict::Action::Kick && !m_moderation->roleCanPerform(m_acl_role, akashi::permission::kick)) {
        l_action = Verdict::Action::Mute;
    }
    if (l_action == Verdict::Action::Mute && !m_moderation->roleCanPerform(m_acl_role, akashi::permission::sanction_mute)) {
        l_action = Verdict::Action::Warn;
    }

    switch (l_action) {
    case Verdict::Action::Warn:
    {
        warnPlayer(f_verdict, QStringLiteral("You have been warned by the automated moderator for %1.").arg(f_verdict.reason));
        const QString l_text = QStringLiteral("warned %1 for %2").arg(f_verdict.ipid, f_verdict.reason);
        notifyModerators(l_text);
        logAction(f_verdict.ipid, l_text);
        break;
    }
    case Verdict::Action::Mute:
    {
        const QDateTime l_until = QDateTime::currentDateTime().addSecs(qint64(f_verdict.mute_minutes) * 60);
        m_moderation->applyTimedSanction(f_verdict.ipid, akashi::sanction::muted, l_until, QStringLiteral("modbot"));
        warnPlayer(f_verdict, QStringLiteral("You have been muted for %1 minutes by the automated moderator for %2.").arg(QString::number(f_verdict.mute_minutes), f_verdict.reason));
        const QString l_text = QStringLiteral("muted %1 for %2 minutes for %3").arg(f_verdict.ipid, QString::number(f_verdict.mute_minutes), f_verdict.reason);
        notifyModerators(l_text);
        logAction(f_verdict.ipid, l_text);
        break;
    }
    case Verdict::Action::Kick:
    {
        if (m_players) {
            const QVector<akashi::ClientSession *> l_clients = m_players->clients();
            for (akashi::ClientSession *l_client : l_clients) {
                akashi::TargetPlayer l_player(l_client);
                if (l_player.ipid() == f_verdict.ipid) {
                    l_player.sendPacket(QStringLiteral("KK"), {QStringLiteral("Kicked by the automated moderator for ") + f_verdict.reason});
                    l_player.closeSocket();
                }
            }
        }
        const QString l_text = QStringLiteral("kicked %1 for %2").arg(f_verdict.ipid, f_verdict.reason);
        notifyModerators(l_text);
        logAction(f_verdict.ipid, l_text);
        break;
    }
    }
}
