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
#include "core/server_context.h"

#include "akashi/config_store.h"
#include "akashi/database_service.h"
#include "akashi/event.h"
#include "akashi/filesystem_service.h"
#include "akashi/log_event.h"
#include "akashi/logging_categories.h"
#include "akashi/network_service.h"
#include "akashi/scheduler.h"
#include "akashi/service_registry.h"
#include "akashi/setting_notifier.h"
#include "akashi/thread_assert.h"
#include "commands/area_commands.h"
#include "commands/authentication_commands.h"
#include "commands/casing_commands.h"
#include "commands/messaging_commands.h"
#include "commands/moderation_commands.h"
#include "commands/music_commands.h"
#include "commands/plugin_commands.h"
#include "commands/roleplay_commands.h"
#include "commands/rule_commands.h"
#include "core/arup_broadcaster.h"
#include "core/auth_throttle.h"
#include "core/authenticator_registry.h"
#include "core/client_session.h"
#include "core/command_registry.h"
#include "core/console_menu.h"
#include "core/core_authenticators.h"
#include "core/crypto_helper.h"
#include "core/db_manager.h"
#include "core/discord_hook.h"
#include "core/log_service.h"
#include "core/medieval_parser.h"
#include "core/permission_registry.h"
#include "core/plugin_manager.h"
#include "core/rule_actions.h"
#include "core/server_publisher.h"
#include "core/server_settings.h"
#include "core/text_filter_registry.h"
#include "core/websocket_receiver.h"
#include "core/writer_text.h"
#include "proto/area_music.h"
#include "proto/chat.h"
#include "proto/handshake.h"
#include "proto/ic.h"
#include "proto/moderation.h"
#include "proto/packet.h"
#include "proto/packet_service.h"
#include "world/area.h"
#include "world/config_loading.h"
#include "world/floor.h"
#include "world/jukebox.h"
#include "world/rule_registry.h"
#include "world/world.h"

#include <QDateTime>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTextStream>

static bool fileExists(const QFileInfo &f) { return f.exists() && f.isFile(); }
static bool dirExists(const QFileInfo &f) { return f.exists() && f.isDir(); }

static bool verifyServerConfig(akashi::ConfigStore *f_store, ServerSettings *f_settings)
{
    auto path = [&](const QString &f) { return f_store->filePath(f); };

    QStringList l_dirs{path(""), path("text/")};
    for (const QString &d : l_dirs) {
        if (!dirExists(QFileInfo(d))) {
            qCCritical(akashiServer) << d + " does not exist!";
            return false;
        }
    }

    // Trigger ini-to-json migration before checking file existence.
    QSettings *l_areas = f_store->settings("areas");

    QStringList l_files{path("config.json"), path("areas.json"), path("backgrounds.txt"),
                        path("characters.txt"), path("music.json"),
                        path("text/8ball.txt"), path("text/gimp.txt"), path("text/praise.txt"),
                        path("text/reprimands.txt"), path("text/cdns.txt"), path("ipbans.json")};
    for (const QString &f : l_files) {
        if (!fileExists(QFileInfo(f))) {
            qCCritical(akashiServer) << f + " does not exist!";
            return false;
        }
    }

    if (l_areas->childGroups().length() < 1) {
        qCCritical(akashiServer) << "areas.json is invalid!";
        return false;
    }

    const int l_soft = f_settings->packet_rate_limit_soft();
    const int l_hard = f_settings->packet_rate_limit_hard();
    if (l_soft > 0 && l_hard <= l_soft) {
        qCritical("packet_rate_limit_hard must be greater than packet_rate_limit_soft!");
        return false;
    }
    if (l_soft <= 0)
        qWarning("packet_rate_limit_soft is 0 or less, warning threshold is disabled!");
    if (l_hard <= 0)
        qWarning("packet_rate_limit_hard is 0 or less, rate limiting is disabled!");

    return true;
}

static int resolveServerPort(akashi::ConfigStore *f_store, ServerSettings *f_settings)
{
    if (f_store->settings("config")->contains("Options/webao_port")) {
        qWarning("webao_port is deprecated, use port instead");
        return f_settings->webao_port();
    }
    return f_settings->port();
}

ExitCode ServerContext::start()
{
    AKASHI_ASSERT_THREAD_AFFINITY();
    setStage(Stage::Configuring);
    m_config_store = new akashi::ConfigStore(akashi::ConfigStore::resolveRootPath(), this);

    auto l_server_settings = new ServerSettings(m_config_store);
    const bool l_valid = l_server_settings->declare();

    m_config_store->settings("acl_roles");

    if (!l_valid || !verifyServerConfig(m_config_store, l_server_settings)) {
        qCCritical(akashiServer) << "The server configuration is invalid!";
        delete l_server_settings;
        return ExitCode::InvalidConfig;
    }

    m_database_service = new akashi::DatabaseService(QStringLiteral("data"), this);
    if (!m_database_service->open(m_config_store->filePath("akashi.db"))) {
        delete l_server_settings;
        return ExitCode::DatabaseError;
    }

    m_services = new akashi::ServiceRegistry(this);
    m_services->registerService(std::make_shared<akashi::FileSystemService>());
    m_services->registerService(std::make_shared<akashi::NetworkService>());
    m_services->registerService(std::shared_ptr<akashi::ConfigStore>(m_config_store, [](auto *) {}));
    m_services->registerService(std::shared_ptr<akashi::DatabaseService>(m_database_service, [](auto *) {}));

    auto l_packets = std::make_shared<akashi::PacketService>();
    akashi::registerHandshakePackets(l_packets->handlers(), l_packets->codecs());
    akashi::registerChatPackets(l_packets->handlers(), l_packets->codecs());
    akashi::registerIcPackets(l_packets->handlers(), l_packets->codecs());
    akashi::registerAreaMusicPackets(l_packets->handlers(), l_packets->codecs());
    akashi::registerModerationPackets(l_packets->handlers(), l_packets->codecs());
    m_services->registerService(l_packets);

    m_port = resolveServerPort(m_config_store, l_server_settings);
    // buildCore constructs its own settings object.
    delete l_server_settings;

    buildCore();
    setStage(Stage::ServicesConstructed);

    ExitCode code = startListening();
    if (code != ExitCode::Ok) {
        return code;
    }

    std::function<bool()> l_busy_check;
    const int l_max_players = m_server_settings->maintenance_max_players();
    if (l_max_players >= 0) {
        l_busy_check = std::bind_front(&ServerContext::isMaintenanceBusy, this, l_max_players);
    }
    connect(m_database_service, &akashi::DatabaseService::maintenanceTriggered,
            logService(), &akashi::LogService::runWriterMaintenance);
    m_scheduler->schedule(
        QStringLiteral("database maintenance"),
        akashi::Schedule::fromDayWord(m_server_settings->maintenance_day(), m_server_settings->maintenance_time()),
        std::bind_front(&ServerContext::runDatabaseMaintenance, this),
        {}, l_busy_check);
    m_scheduler->schedule(
        QStringLiteral("database backups"),
        akashi::Schedule::fromDayWord(m_server_settings->backup_day(), m_server_settings->backup_time()),
        std::bind_front(&ServerContext::runDatabaseBackups, this));
    registerConsoleTasks();
    armStoredSanctions();
    setStage(Stage::ContentLoaded);

    const QString l_plugin_dir = QDir(m_config_store->rootPath()).absoluteFilePath(QStringLiteral("../plugins"));
    m_plugin_manager = new akashi::PluginManager(m_services, l_plugin_dir, this);
    m_services->registerService(std::shared_ptr<akashi::PluginManager>(m_plugin_manager, [](auto *) {}));
    // Applied rules hold functions built by plugin code; the server drops
    // them before the plugin's library leaves memory.
    connect(m_plugin_manager, &akashi::PluginManager::pluginAboutToUnload,
            this, &ServerContext::onPluginAboutToUnload);
    // A plugin loaded at runtime registers commands the owner's extensions
    // may address; re-applying is idempotent.
    connect(m_plugin_manager, &akashi::PluginManager::pluginLoaded, this, &ServerContext::onPluginLoaded);

    QStringList l_plugin_allowlist;
    const QString l_allowlist_path = m_config_store->filePath(QStringLiteral("plugins.ini"));
    QFile l_allowlist_file(l_allowlist_path);
    if (l_allowlist_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream l_stream(&l_allowlist_file);
        while (!l_stream.atEnd()) {
            QString l_line = l_stream.readLine().trimmed();
            if (!l_line.isEmpty() && !l_line.startsWith(QLatin1Char('#')))
                l_plugin_allowlist.append(l_line);
        }
    }

    m_plugin_manager->startPlugins(l_plugin_allowlist);
    m_command_registry->applyExtensions(configPath("command_extensions.json"));
    setStage(Stage::PluginsLoaded);

    // The one active authentication system resolves after the plugins
    // started, so a plugin-provided system can be the configured one. The
    // socket already listens, but start() runs in a single event loop
    // turn: no connection is serviced before this resolves.
    if (!resolveActiveAuthenticator()) {
        return ExitCode::InvalidConfig;
    }
    bootstrapRootAccount();
    bootstrapModpass();

    setStage(Stage::Listening);
    setStage(Stage::Running);
    return ExitCode::Ok;
}

QString ServerContext::configuredAuthSystemId() const
{
    const QString l_configured = m_server_settings->auth().trimmed();
    if (l_configured.compare(QLatin1String("simple"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("password");
    }
    if (l_configured.compare(QLatin1String("advanced"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("username");
    }
    return l_configured;
}

bool ServerContext::resolveActiveAuthenticator()
{
    const QString l_system_id = configuredAuthSystemId();
    m_active_authenticator = m_authenticator_registry->authenticatorFor(l_system_id);
    if (!m_active_authenticator) {
        // Never fall back to a weaker system: an unresolvable id stops the
        // server before anybody can log in.
        qCCritical(akashiServer).noquote() << QStringLiteral("No authentication system named \"%1\" is registered; the server cannot start. Check the auth setting, or load the plugin that provides it.").arg(l_system_id);
        return false;
    }
    m_active_auth_system_id = m_active_authenticator->systemId();
    return true;
}

QString ServerContext::generateBootstrapSecret() const
{
    // Hex-spelled random secret as long as the configured minimum password
    // length demands, floored at the setting's own default for the
    // degenerate zero case.
    const int l_length = qMax(m_server_settings->pass_min_length(), 8);
    return QString::fromLatin1(CryptoHelper::randbytes((l_length + 1) / 2).toHex()).left(l_length);
}

void ServerContext::bootstrapRootAccount()
{
    if (m_active_auth_system_id != QStringLiteral("username")) {
        return;
    }
    if (db_manager->fetchCredentials(QStringLiteral("root"))) {
        return;
    }

    const QString l_password = generateBootstrapSecret();
    if (!db_manager->createUser(QStringLiteral("root"), CryptoHelper::randbytes(16), l_password, akashi::ACLRolesHandler::SUPER_ID)) {
        qCCritical(akashiServer) << "Unable to create the root account; see the database log.";
        return;
    }

    // Console only - this banner must never route through the LogService,
    // so the password cannot land in a log file.
    qCInfo(akashiServer).noquote() << QStringLiteral(
                                          "\n==================================================================\n"
                                          "  No root account existed, so one was created.\n"
                                          "  Username: root\n"
                                          "  Password: %1\n"
                                          "  Log in with /login root <password> and change the password in\n"
                                          "  the console's authentication menu or with /changepass.\n"
                                          "  This password is shown once and never logged.\n"
                                          "==================================================================")
                                          .arg(l_password);
}

void ServerContext::bootstrapModpass()
{
    if (m_active_auth_system_id != QStringLiteral("password")) {
        return;
    }
    if (!m_server_settings->modpass().isEmpty()) {
        return;
    }

    // The same ConfigStore write path as the console menu's modpass item:
    // live immediately, persisted to config.json.
    const QString l_modpass = generateBootstrapSecret();
    m_server_settings->modpass.set(l_modpass);
    m_config_store->settings(QStringLiteral("config"))->sync();

    // Console only - this banner must never route through the LogService,
    // so the modpass cannot land in a log file.
    qCInfo(akashiServer).noquote() << QStringLiteral(
                                          "\n==================================================================\n"
                                          "  No modpass was set, so one was generated.\n"
                                          "  Modpass: %1\n"
                                          "  Log in with /login and change it in the console's\n"
                                          "  authentication menu; it is stored in config.json.\n"
                                          "==================================================================")
                                          .arg(l_modpass);
}

std::shared_ptr<akashi::Authenticator> ServerContext::activeAuthenticator() const
{
    return m_active_authenticator;
}

QString ServerContext::activeAuthSystemId() const
{
    return m_active_auth_system_id;
}

void ServerContext::deliverAuthOutcome(QPointer<akashi::ClientSession> f_session, const akashi::AuthOutcome &f_outcome)
{
    // Client ids get reused; only the QPointer proves the session that
    // asked is still the one answering the door.
    if (!f_session) {
        return;
    }
    f_session->onAuthOutcome(f_outcome);
}

void ServerContext::buildCore()
{
    m_player_count = 0;

    m_server_settings = new ServerSettings(m_config_store);
    m_areas_ini = m_config_store->settings("areas");

    m_text_data.magic_8ball = akashi::config::loadTextFile(configPath("text/8ball.txt"));
    m_text_data.praises = akashi::config::loadTextFile(configPath("text/praise.txt"));
    m_text_data.reprimands = akashi::config::loadTextFile(configPath("text/reprimands.txt"));
    m_text_data.gimps = akashi::config::loadTextFile(configPath("text/gimp.txt"));
    m_text_data.filters = akashi::config::loadTextFile(configPath("text/filter.txt"));
    m_text_data.compiled_filters.clear();
    for (const QString &l_pattern : std::as_const(m_text_data.filters))
        m_text_data.compiled_filters.append(QRegularExpression(l_pattern, QRegularExpression::CaseInsensitiveOption));
    m_text_data.cdns = akashi::config::loadTextFile(configPath("text/cdns.txt"));
    if (m_text_data.cdns.isEmpty())
        m_text_data.cdns = QStringList{"cdn.discord.com"};

    QSettings &l_dice_ini = *m_config_store->settings("dice");
    const QStringList l_dices = l_dice_ini.childGroups();
    for (const QString &dice : l_dices) {
        l_dice_ini.beginGroup(dice);
        int max = l_dice_ini.value("max").toInt();
        QStringList faces;
        for (int i = 1; i <= max; ++i) {
            QString key = QString::number(i);
            if (l_dice_ini.contains(key)) {
                faces.append(l_dice_ini.value(key).toString());
            }
            else {
                qCCritical(akashiServer) << "dice.ini max mismatch!";
                break;
            }
        }
        m_text_data.dice_faces[dice] = faces;
        l_dice_ini.endGroup();
    }

    timer = new QTimer(this);

    if (m_services) {
        m_packets = m_services->resolve<akashi::PacketService>("akashi.packets");
    }
    m_filesystem = m_services->resolve<akashi::FileSystemService>("akashi.filesystem").get();
    db_manager = new DBManager(m_database_service->database());
    acl_roles_handler = new akashi::ACLRolesHandler(this);
    acl_roles_handler->loadFile(configPath("acl_roles.json"));

    m_permission_registry = new akashi::PermissionRegistry;
    m_command_registry = new akashi::CommandRegistry;
    m_rule_registry = new akashi::RuleRegistry;
    akashi::registerCoreRuleActions(this, m_rule_registry);

    m_console_menu = new akashi::ConsoleMenu(this, this);
    m_services->registerService(std::shared_ptr<akashi::ConsoleMenu>(m_console_menu, [](auto *) {}));

    m_scheduler = new akashi::Scheduler(this);
    m_services->registerService(std::shared_ptr<akashi::Scheduler>(m_scheduler, [](auto *) {}));

    auto l_network_service = m_services->resolve<akashi::NetworkService>(QStringLiteral("akashi.network"));
    m_discord_hook = new akashi::DiscordHook(l_network_service ? l_network_service->networkManager() : nullptr, this);
    m_services->registerService(std::shared_ptr<akashi::DiscordHook>(m_discord_hook, [](auto *) {}));

    m_world = new akashi::World(m_rule_registry, m_services, m_filesystem, m_areas_ini, this);
    m_services->registerService(std::shared_ptr<akashi::World>(m_world, [](auto *) {}));
    connect(m_world, &akashi::World::areaBuilt, this, &ServerContext::onAreaBuilt);
    connect(m_world, &akashi::World::areaAboutToBeRemoved, this, &ServerContext::onAreaAboutToBeRemoved);

    m_text_filter_registry = new akashi::TextFilterRegistry;

    // The word filter guards both chat surfaces; the sanction filters
    // below stay IC-only, so /gimp and friends never touch OOC.
    m_text_filter_registry->registerFilter(
        QStringLiteral("word-filter"), 100, std::bind_front(&ServerContext::applyWordFilter, this), true, QStringLiteral("core"), {akashi::TextChannel::Ic, akashi::TextChannel::Ooc});

    m_text_filter_registry->registerFilter(
        QStringLiteral("gimped"), 200, std::bind_front(&ServerContext::applyGimpFilter, this), false, QStringLiteral("core"));

    m_medieval_parser = std::make_unique<MedievalParser>(configPath("text/autorp.json"));
    m_text_filter_registry->registerFilter(
        QStringLiteral("medieval"), 300, std::bind_front(&ServerContext::applyMedievalFilter, this), false, QStringLiteral("core"));

    m_text_filter_registry->registerFilter(
        QStringLiteral("shaken"), 400, &ServerContext::applyShakeFilter, false, QStringLiteral("core"));

    m_text_filter_registry->registerFilter(
        QStringLiteral("disemvoweled"), 500, &ServerContext::applyDisemvowelFilter, false, QStringLiteral("core"));

    m_auth_throttle = new akashi::AuthThrottle(m_server_settings->max_login_attempts(),
                                               m_server_settings->login_lockout_seconds());

    // Every authentication system the server could run; plugins add their
    // own before the launch-time resolution picks the one active system.
    m_authenticator_registry = new akashi::AuthenticatorRegistry;
    m_authenticator_registry->registerAuthenticator(
        std::make_shared<akashi::PasswordAuthenticator>(m_server_settings), QStringLiteral("core"));
    m_authenticator_registry->registerAuthenticator(
        std::make_shared<akashi::UsernameAuthenticator>(db_manager), QStringLiteral("core"));

    m_log_service = new akashi::LogService(m_config_store, m_server_settings->logbuffer(), this);

    const QString l_log_mode = m_server_settings->logging().toLower();
    akashi::WriterText::Mode l_writer_mode = akashi::WriterText::Mode::Modcall;
    if (l_log_mode == QStringLiteral("full")) {
        l_writer_mode = akashi::WriterText::Mode::Full;
    }
    else if (l_log_mode == QStringLiteral("fullarea")) {
        l_writer_mode = akashi::WriterText::Mode::FullArea;
    }
    m_text_writer = std::make_shared<akashi::WriterText>(l_writer_mode, m_log_service);
    m_log_service->registerWriter(m_text_writer, QStringLiteral("core"));

    m_services->registerService(std::shared_ptr<akashi::CommandRegistry>(m_command_registry, [](auto *) {}));
    m_services->registerService(std::shared_ptr<akashi::PermissionRegistry>(m_permission_registry, [](auto *) {}));
    m_services->registerService(std::shared_ptr<akashi::LogService>(m_log_service, [](auto *) {}));
    m_services->registerService(std::shared_ptr<akashi::TextFilterRegistry>(m_text_filter_registry, [](auto *) {}));
    m_services->registerService(std::shared_ptr<akashi::RuleRegistry>(m_rule_registry, [](auto *) {}));
    m_services->registerService(std::shared_ptr<akashi::AuthenticatorRegistry>(m_authenticator_registry, [](auto *) {}));
}

std::optional<QString> ServerContext::applyWordFilter(const QString &f_text) const
{
    QString l_result = f_text;
    for (const QRegularExpression &l_re : std::as_const(m_text_data.compiled_filters))
        l_result.replace(l_re, QStringLiteral("❌"));
    return l_result;
}

std::optional<QString> ServerContext::applyGimpFilter(const QString &) const
{
    const auto &l_list = gimpList();
    return l_list.at(QRandomGenerator::global()->bounded(l_list.size()));
}

std::optional<QString> ServerContext::applyMedievalFilter(const QString &f_text) const
{
    return m_medieval_parser->degrootify(f_text);
}

std::optional<QString> ServerContext::applyShakeFilter(const QString &f_text)
{
    QStringList l_words = f_text.split(QStringLiteral(" "));
    std::shuffle(l_words.begin(), l_words.end(), *QRandomGenerator::global());
    return l_words.join(QStringLiteral(" "));
}

std::optional<QString> ServerContext::applyDisemvowelFilter(const QString &f_text)
{
    static const QRegularExpression l_vowels(QStringLiteral("[AEIOUaeiou]"));
    return QString(f_text).remove(l_vowels);
}

void ServerContext::onPluginLoaded()
{
    m_command_registry->applyExtensions(configPath("command_extensions.json"));
}

void ServerContext::registerConsoleTasks()
{
    m_console_menu->registerScheduledAction(
        QStringLiteral("database maintenance"),
        std::bind_front(&ServerContext::nextScheduledRun, this, QStringLiteral("database maintenance")),
        std::bind_front(&ServerContext::runMaintenanceAndReport, this));

    m_console_menu->registerScheduledAction(
        QStringLiteral("database backups"),
        std::bind_front(&ServerContext::nextScheduledRun, this, QStringLiteral("database backups")),
        std::bind_front(&ServerContext::runBackupsAndReport, this));

    m_console_menu->registerAction(QStringLiteral("show recent bans"), std::bind_front(&ServerContext::showRecentBans, this));

    m_console_menu->registerAction(QStringLiteral("show database overview"), std::bind_front(&ServerContext::showDatabaseOverview, this));
}

bool ServerContext::isMaintenanceBusy(int f_max_players)
{
    return playerCount() > f_max_players;
}

void ServerContext::runDatabaseMaintenance()
{
    m_database_service->runMaintenanceNow(m_server_settings->maintenance_vacuum());
}

void ServerContext::runDatabaseBackups()
{
    m_database_service->runBackups(m_server_settings->backup_keep());
}

std::optional<QDateTime> ServerContext::nextScheduledRun(const QString &f_job_id) const
{
    return m_scheduler->nextRunAt(f_job_id);
}

void ServerContext::runMaintenanceAndReport()
{
    m_database_service->runMaintenanceNow(m_server_settings->maintenance_vacuum());
    if (auto *l_session = akashi::ConsoleMenu::activeSession()) {
        l_session->printOut(QStringLiteral("Maintenance finished; timings are in the server log."));
    }
}

void ServerContext::runBackupsAndReport()
{
    const int l_written = m_database_service->runBackups(m_server_settings->backup_keep());
    if (auto *l_session = akashi::ConsoleMenu::activeSession()) {
        l_session->printOut(QStringLiteral("Backed up %1 database(s).").arg(l_written));
    }
}

void ServerContext::showRecentBans()
{
    auto *l_session = akashi::ConsoleMenu::activeSession();
    if (!l_session) {
        return;
    }
    const QList<DBManager::BanInfo> l_bans = db_manager->recentBans();
    if (l_bans.isEmpty()) {
        l_session->printOut(QStringLiteral("No bans on record."));
        return;
    }
    for (const DBManager::BanInfo &l_ban : l_bans) {
        const QDateTime l_issued = QDateTime::fromSecsSinceEpoch(l_ban.time);
        const QString l_until = l_ban.duration == -2
                                    ? QStringLiteral("permanent")
                                    : l_issued.addSecs(l_ban.duration).toString(QStringLiteral("yyyy-MM-dd hh:mm"));
        l_session->printOut(QStringLiteral("#%1 | %2 | banned %3 | until %4 | by %5 | %6")
                                .arg(l_ban.id)
                                .arg(l_ban.ipid,
                                     l_issued.toString(QStringLiteral("yyyy-MM-dd hh:mm")),
                                     l_until, l_ban.moderator, l_ban.reason));
    }
}

void ServerContext::showDatabaseOverview()
{
    auto *l_session = akashi::ConsoleMenu::activeSession();
    if (!l_session) {
        return;
    }
    const QStringList l_lines = m_database_service->overview();
    for (const QString &l_line : l_lines) {
        l_session->printOut(l_line);
    }
}

static QString sanctionJobId(const QString &f_ipid, const QString &f_sanction)
{
    return QStringLiteral("lift %1 %2").arg(f_sanction, f_ipid);
}

void ServerContext::applyTimedSanction(const QString &f_ipid, const QString &f_sanction, const QDateTime &f_until, const QString &f_moderator)
{
    const QList<akashi::ClientSession *> l_clients = clientsByIpid(f_ipid);
    for (akashi::ClientSession *l_client : l_clients) {
        l_client->setSanction(f_sanction, true);
    }
    db_manager->upsertSanction({f_ipid, f_sanction, f_moderator,
                                QDateTime::currentSecsSinceEpoch(), f_until.toSecsSinceEpoch()});
    scheduleSanctionLift(f_ipid, f_sanction, f_until);
}

void ServerContext::clearStoredSanction(const QString &f_ipid, const QString &f_sanction)
{
    db_manager->removeSanction(f_ipid, f_sanction);
    m_scheduler->cancel(sanctionJobId(f_ipid, f_sanction));
}

void ServerContext::scheduleSanctionLift(const QString &f_ipid, const QString &f_sanction, const QDateTime &f_until)
{
    m_scheduler->schedule(
        sanctionJobId(f_ipid, f_sanction),
        akashi::Schedule::once(f_until),
        std::bind_front(&ServerContext::liftSanction, this, f_ipid, f_sanction));
}

void ServerContext::liftSanction(const QString &f_ipid, const QString &f_sanction)
{
    db_manager->removeSanction(f_ipid, f_sanction);
    const QList<akashi::ClientSession *> l_clients = clientsByIpid(f_ipid);
    for (akashi::ClientSession *l_client : l_clients) {
        l_client->setSanction(f_sanction, false);
        l_client->sendServerMessage(QStringLiteral("Your \"%1\" sanction has expired.").arg(f_sanction));
    }
    qCInfo(akashiServer).noquote() << QStringLiteral("Sanction \"%1\" on %2 expired.").arg(f_sanction, f_ipid);
}

void ServerContext::armStoredSanctions()
{
    const QList<DBManager::SanctionInfo> l_stored = db_manager->allSanctions();
    for (const DBManager::SanctionInfo &l_sanction : l_stored) {
        scheduleSanctionLift(l_sanction.ipid, l_sanction.sanction, QDateTime::fromSecsSinceEpoch(l_sanction.expires));
    }
    if (!l_stored.isEmpty()) {
        qCInfo(akashiServer) << "Armed" << l_stored.size() << "stored sanction lift(s).";
    }
}

void ServerContext::applyStoredSanctions(akashi::ClientSession *f_client)
{
    const QList<DBManager::SanctionInfo> l_active = db_manager->sanctionsFor(f_client->ipid(), QDateTime::currentSecsSinceEpoch());
    for (const DBManager::SanctionInfo &l_sanction : l_active) {
        f_client->setSanction(l_sanction.sanction, true);
    }
}

ExitCode ServerContext::startListening()
{
    QString bind_ip = m_server_settings->bind_ip();
    QHostAddress bind_addr;
    if (bind_ip == "all")
        bind_addr = QHostAddress::Any;
    else
        bind_addr = QHostAddress(bind_ip);
    if (bind_addr.protocol() != QAbstractSocket::IPv4Protocol && bind_addr.protocol() != QAbstractSocket::IPv6Protocol && bind_addr != QHostAddress::Any) {
        qCCritical(akashiServer) << bind_ip << "is an invalid IP address to listen on! Server not starting, check your config.";
        return ExitCode::InvalidBindAddress;
    }

    // The configured auth id stands in for the active one, which
        // resolves only after the plugins start - a config naming an
        // unloadable system stops the server anyway.
        QStringList l_vocabulary = akashi::serverFeatures();
        l_vocabulary.append(QStringLiteral("auth_") + configuredAuthSystemId());
        m_receiver = new akashi::WebSocketReceiver(bind_addr, m_port, l_vocabulary, this);
    connect(m_receiver, &akashi::ClientReceiver::inboundClient, this, &ServerContext::inboundClient);
    if (!m_receiver->start()) {
        qCCritical(akashiServer) << "Server error:" << m_receiver->lastError();
        return ExitCode::PortUnavailable;
    }
    qCInfo(akashiServer) << "Server listening on" << m_receiver->port();

    // Construct modern advertiser if enabled in config
    server_publisher = new ServerPublisher(m_receiver->port(), &m_player_count, m_server_settings, this);

    m_characters = akashi::config::loadTextFile(configPath("characters.txt"));
    m_backgrounds = akashi::config::loadTextFile(configPath("backgrounds.txt"));

    // Seed the default floor's music catalog from music.json.
    auto l_music = akashi::config::loadMusicList(configPath("music.json"));
    akashi::Floor &l_default_floor = m_world->defaultFloor();
    l_default_floor.music_ordered = l_music.ordered;
    l_default_floor.approved_cdns = m_text_data.cdns;
    for (auto it = l_music.songs.constBegin(); it != l_music.songs.constEnd(); ++it) {
        l_default_floor.music_songs.insert(it.key(), {it.key(), it.value().first, it.value().second});
    }

    // The broadcaster exists before the world builds, so onAreaBuilt wires
    // every area the same way at startup and at runtime.
    m_arup_broadcaster = new akashi::ArupBroadcaster(this);
    m_arup_broadcaster->setOwnerFormatter(std::bind_front(&ServerContext::formatAreaOwner, this));
    connect(m_arup_broadcaster, &akashi::ArupBroadcaster::arupFloorBroadcast, this, &ServerContext::onArupFloorBroadcast);
    connect(m_arup_broadcaster, &akashi::ArupBroadcaster::arupUnicast, this, &ServerContext::unicast);

    m_world->buildFromConfig(QFileInfo(configPath("areas.json")).absoluteFilePath());

    m_ipban_list = akashi::config::loadIpRangeBans(configPath("ipbans.json"));
    m_banned_asns = akashi::config::loadBannedAsns(configPath("ipbans.json"));
    if (QFile::exists("storage/GeoLite2-ASN.mmdb")) {
        m_asn_reader.open("storage/GeoLite2-ASN.mmdb");
    }

    // Text data must reload before the music floor (cdns feed into it).
    connect(m_config_store, &akashi::ConfigStore::configReloaded, this, &ServerContext::reloadTextData);
    connect(m_config_store, &akashi::ConfigStore::configReloaded, this, &ServerContext::reloadMusicFloor);
    connect(m_config_store, &akashi::ConfigStore::configReloaded, this, &ServerContext::reloadBanLists);
    connect(m_config_store, &akashi::ConfigStore::configReloaded, m_log_service, &akashi::LogService::reloadTemplates);
    connect(m_config_store, &akashi::ConfigStore::configReloaded, this, &ServerContext::reloadAclRoles);
    connect(m_config_store, &akashi::ConfigStore::configReloaded, this, &ServerContext::reloadAuthLimits);
    connect(m_config_store, &akashi::ConfigStore::configReloaded, this, &ServerContext::warnOnAuthSystemChange);
    // Observers hear about the reload last, after core re-read everything.
    connect(m_config_store, &akashi::ConfigStore::configReloaded, this, &ServerContext::publishConfigReloaded);

    m_player_directory.setCapacity(m_server_settings->max_players());
    applyIdAssignment();
    connect(m_server_settings->id_assignment.notifier(), &akashi::SettingNotifier::changed, this, &ServerContext::applyIdAssignment);

    // Register built-in permissions.
    const auto l_register_perm = [this](const QString &f_id, const QString &f_display, const QString &f_category) {
        m_permission_registry->registerPermission({f_id, f_display, {}, f_category}, QStringLiteral("core"));
    };
    l_register_perm(akashi::permission::kick, QStringLiteral("Kick"), QStringLiteral("moderation"));
    l_register_perm(akashi::permission::ban, QStringLiteral("Ban"), QStringLiteral("moderation"));
    l_register_perm(akashi::permission::lock_background, QStringLiteral("Lock Background"), QStringLiteral("area"));
    l_register_perm(akashi::permission::modify_users, QStringLiteral("Modify Users"), QStringLiteral("administration"));
    l_register_perm(akashi::permission::gamemaster, QStringLiteral("Case Manager"), QStringLiteral("area"));
    l_register_perm(akashi::permission::global_timer, QStringLiteral("Global Timer"), QStringLiteral("area"));
    l_register_perm(akashi::permission::modify_evidence, QStringLiteral("Modify Evidence"), QStringLiteral("area"));
    l_register_perm(akashi::permission::motd, QStringLiteral("MOTD"), QStringLiteral("administration"));
    l_register_perm(akashi::permission::announcer, QStringLiteral("Announcer"), QStringLiteral("moderation"));
    l_register_perm(akashi::permission::chat_moderator, QStringLiteral("Chat Moderator"), QStringLiteral("moderation"));
    l_register_perm(akashi::permission::mute, QStringLiteral("Mute"), QStringLiteral("moderation"));
    l_register_perm(akashi::permission::remove_gamemaster, QStringLiteral("Remove CM"), QStringLiteral("moderation"));
    l_register_perm(akashi::permission::save_testimony, QStringLiteral("Save Testimony"), QStringLiteral("area"));
    l_register_perm(akashi::permission::force_charselect, QStringLiteral("Force Charselect"), QStringLiteral("moderation"));
    l_register_perm(akashi::permission::bypass_locks, QStringLiteral("Bypass Locks"), QStringLiteral("moderation"));
    l_register_perm(akashi::permission::ignore_background_list, QStringLiteral("Ignore BG List"), QStringLiteral("area"));
    l_register_perm(akashi::permission::send_notice, QStringLiteral("Send Notice"), QStringLiteral("moderation"));
    l_register_perm(akashi::permission::jukebox, QStringLiteral("Jukebox"), QStringLiteral("area"));
    l_register_perm(akashi::permission::modify_rules, QStringLiteral("Modify Rules"), QStringLiteral("area"));
    l_register_perm(akashi::permission::modify_floors, QStringLiteral("Modify Floors"), QStringLiteral("administration"));
    l_register_perm(akashi::permission::super, QStringLiteral("Super"), QStringLiteral("administration"));
    l_register_perm(akashi::permission::user, QStringLiteral("User"), QStringLiteral("lifecycle"));
    l_register_perm(akashi::permission::bypass_rules, QStringLiteral("Bypass Rules"), QStringLiteral("moderation"));

    // Register the built-in permission resolver chain.
    m_permission_registry->registerResolver(
        QStringLiteral("none_check"), 0, &ServerContext::resolveNoneCheck, QStringLiteral("core"));
    m_permission_registry->registerResolver(
        QStringLiteral("joined_user"), 50, std::bind_front(&ServerContext::resolveJoinedUser, this), QStringLiteral("core"));
    m_permission_registry->registerResolver(
        QStringLiteral("area_owner"), 100, std::bind_front(&ServerContext::resolveAreaOwner, this), QStringLiteral("core"));
    m_permission_registry->registerResolver(
        QStringLiteral("authentication"), 200, &ServerContext::resolveAuthentication, QStringLiteral("core"));
    m_permission_registry->registerResolver(
        QStringLiteral("role_check"), 300, std::bind_front(&ServerContext::resolveRoleCheck, this), QStringLiteral("core"));

    akashi::commands::registerAreaCommands(*m_command_registry);
    akashi::commands::registerAuthenticationCommands(*m_command_registry);
    akashi::commands::registerModerationCommands(*m_command_registry);
    akashi::commands::registerCasingCommands(*m_command_registry);
    akashi::commands::registerMusicCommands(*m_command_registry);
    akashi::commands::registerRoleplayCommands(*m_command_registry);
    akashi::commands::registerMessagingCommands(*m_command_registry);
    akashi::commands::registerPluginCommands(*m_command_registry);
    akashi::commands::registerRuleCommands(*m_command_registry);
    // The command extensions apply after the plugins load, so they can
    // reach plugin commands too.

    return ExitCode::Ok;
}

akashi::PermissionVerdict ServerContext::resolveNoneCheck(const akashi::PermissionQuery &f_query)
{
    if (f_query.permission.isEmpty() || f_query.permission == akashi::permission::none) {
        return akashi::PermissionVerdict::Granted;
    }
    return akashi::PermissionVerdict::NoOpinion;
}

// The user tier: granted by having finished the join handshake, no
// account needed. Lifecycle is a fact, so nothing here can deny.
akashi::PermissionVerdict ServerContext::resolveJoinedUser(const akashi::PermissionQuery &f_query)
{
    if (f_query.permission == akashi::permission::user) {
        akashi::ClientSession *l_client = clientById(f_query.client_id);
        if (l_client && l_client->stage() == akashi::ClientSession::SessionStage::Joined) {
            return akashi::PermissionVerdict::Granted;
        }
    }
    return akashi::PermissionVerdict::NoOpinion;
}

akashi::PermissionVerdict ServerContext::resolveAreaOwner(const akashi::PermissionQuery &f_query)
{
    if (f_query.permission == akashi::permission::gamemaster) {
        akashi::Area *l_area = areaById(f_query.area_id);
        if (l_area && l_area->owners().contains(f_query.client_id)) {
            return akashi::PermissionVerdict::Granted;
        }
    }
    return akashi::PermissionVerdict::NoOpinion;
}

akashi::PermissionVerdict ServerContext::resolveAuthentication(const akashi::PermissionQuery &f_query)
{
    if (!f_query.is_authenticated) {
        return akashi::PermissionVerdict::Denied;
    }
    if (f_query.auth_type == QStringLiteral("simple")) {
        return akashi::PermissionVerdict::Granted;
    }
    return akashi::PermissionVerdict::NoOpinion;
}

akashi::PermissionVerdict ServerContext::resolveRoleCheck(const akashi::PermissionQuery &f_query)
{
    const akashi::ACLRole l_role = acl_roles_handler->roleById(f_query.acl_role_id);
    if (l_role.canPerform(f_query.permission)) {
        return akashi::PermissionVerdict::Granted;
    }
    return akashi::PermissionVerdict::Denied;
}

QString ServerContext::formatAreaOwner(int f_owner_id)
{
    akashi::ClientSession *l_owner = clientById(f_owner_id);
    if (!l_owner) {
        return {};
    }
    return QStringLiteral("[") + QString::number(l_owner->clientId()) + QStringLiteral("] ") + l_owner->character();
}

void ServerContext::onArupFloorBroadcast(const akashi::Packet &f_packet, int f_floor_id)
{
    const akashi::Floor *l_floor = floorById(f_floor_id);
    if (!l_floor)
        return;
    for (int l_aid : l_floor->area_ids) {
        broadcast(f_packet, l_aid);
    }
}

void ServerContext::applyIdAssignment()
{
    m_player_directory.setIdAssignment(m_server_settings->id_assignment() == "lowest"
                                           ? PlayerDirectory::IdAssignment::Lowest
                                           : PlayerDirectory::IdAssignment::LastFreed);
}

QVector<akashi::ClientSession *> ServerContext::clients()
{
    return m_player_directory.clients();
}

void ServerContext::inboundClient(akashi::ITransport *f_transport)
{
    akashi::ITransport *l_socket = f_transport;

    // A connection that died while queued emitted its signals before anyone
    // listened; without this check it would linger as a phantom client
    // forever, holding a player slot with no connection left to tear it down.
    if (!l_socket->isOpen()) {
        l_socket->deleteLater();
        return;
    }

    // When the server is full, a client waiting on a reconnect gives up its
    // place first: a person actually arriving beats one who may come back.
    if (m_player_directory.isFull()) {
        akashi::ClientSession *l_waiting_client = nullptr;
        const QVector<akashi::ClientSession *> l_current_clients = m_player_directory.clients();
        for (akashi::ClientSession *i_client : l_current_clients) {
            if (i_client->isWaitingForReconnect()) {
                l_waiting_client = i_client;
                break;
            }
        }
        if (l_waiting_client) {
            removeClient(l_waiting_client);
        }
    }

    // Too many players. Reject connection!
    // This also enforces the maximum playercount.
    if (m_player_directory.isFull()) {
        akashi::Packet disconnect_reason("BD", {"Maximum playercount has been reached."});
        l_socket->write(disconnect_reason);
        connect(l_socket, &akashi::ITransport::clientDisconnected, l_socket, &QObject::deleteLater);
        l_socket->close();
        return;
    }

    int user_id = m_player_directory.takeId();
    akashi::ClientSession *client = new akashi::ClientSession(this, l_socket, user_id);

    int multiclient_count = 1;
    bool is_at_multiclient_limit = false;
    client->calculateIpid();
    auto ban = db_manager->isIPBanned(client->ipid());
    bool is_banned = ban.first;
    const QVector<akashi::ClientSession *> l_connected_clients = m_player_directory.clients();
    for (akashi::ClientSession *joined_client : l_connected_clients) {
        if (client->remoteIp().isEqual(joined_client->remoteIp()))
            multiclient_count++;
    }

    if (multiclient_count > m_server_settings->multiclient_limit() && !client->remoteIp().isLoopback())
        is_at_multiclient_limit = true;

    if (is_banned) {
        QString ban_duration;
        if (!(ban.second.duration == -2)) {
            ban_duration = QDateTime::fromSecsSinceEpoch(ban.second.time).addSecs(ban.second.duration).toString("MM/dd/yyyy, hh:mm");
        }
        else {
            ban_duration = "Permanently.";
        }
        akashi::Packet ban_reason("BD", {"Reason: " + ban.second.reason + "\nBan ID: " + QString::number(ban.second.id) + "\nUntil: " + ban_duration});
        l_socket->write(ban_reason);
    }
    if (is_banned || is_at_multiclient_limit) {
        m_player_directory.returnId(user_id);
        // The client is deleted once the transport reports the close, so the
        // rejection message still flushes; the transport bounds the wait.
        connect(l_socket, &akashi::ITransport::clientDisconnected, client, &QObject::deleteLater);
        l_socket->close();
        return;
    }

    QHostAddress l_remote_ip = client->remoteIp();
    if (l_remote_ip.protocol() == QAbstractSocket::IPv6Protocol) {
        l_remote_ip = parseToIPv4(l_remote_ip);
    }

    if (isIPBanned(l_remote_ip)) {
        QString l_reason = "Your IP has been banned by a moderator.";
        akashi::Packet l_ban_reason("BD", {l_reason});
        l_socket->write(l_ban_reason);
        m_player_directory.returnId(user_id);
        connect(l_socket, &akashi::ITransport::clientDisconnected, client, &QObject::deleteLater);
        l_socket->close();
        return;
    }

    m_player_directory.addClient(user_id, client);
    applyStoredSanctions(client);
    connect(client, &akashi::ClientSession::transportClosed, this, std::bind_front(&ServerContext::onClientTransportClosed, this, client));
    connect(client, &akashi::ClientSession::reconnectTimedOut, this, std::bind_front(&ServerContext::removeClient, this, client));

    // This is the infamous workaround for
    // tsuserver4. It should disable fantacrypt
    // completely in any client 2.4.3 or newer
    akashi::Packet decryptor("decryptor", {"NOENCRYPT"});
    client->sendPacket(decryptor);
    connect(client, &akashi::ClientSession::joined, this, &ServerContext::increasePlayerCount);
}

void ServerContext::onClientTransportClosed(akashi::ClientSession *f_client, akashi::DisconnectKind f_kind)
{
    // A lost connection may be a network problem the person comes right
    // back from: keep the client and its characters for the grace
    // period. A clean close, a client that never joined, or no grace
    // configured means removal right away.
    const int l_grace_seconds = m_server_settings->reconnect_grace();
    if (f_kind == akashi::DisconnectKind::Lost && l_grace_seconds > 0 && f_client->stage() == akashi::ClientSession::SessionStage::Joined) {
        f_client->waitForReconnect(l_grace_seconds);
        return;
    }
    removeClient(f_client);
}

void ServerContext::updateCharsTaken(akashi::Area *area)
{
    QStringList chars_taken;
    for (const QString &cur_char : qAsConst(m_characters)) {
        chars_taken.append(area->charactersTaken().contains(characterId(cur_char))
                               ? QStringLiteral("-1")
                               : QStringLiteral("0"));
    }

    akashi::Packet response_cc("CharsCheck", chars_taken);

    const QVector<akashi::ClientSession *> l_client_list = m_player_directory.clients();
    for (akashi::ClientSession *l_client : l_client_list) {
        if (l_client->areaId() == area->id()) {
            if (!l_client->isCharCursed())
                l_client->sendPacket(response_cc);
            else {
                QStringList chars_taken_cursed = cursedCharsTaken(l_client, chars_taken);
                akashi::Packet response_cc_cursed("CharsCheck", chars_taken_cursed);
                l_client->sendPacket(response_cc_cursed);
            }
        }
    }
}

QStringList ServerContext::cursedCharsTaken(akashi::ClientSession *client, QStringList chars_taken)
{
    QStringList chars_taken_cursed;
    for (int i = 0; i < chars_taken.length(); i++) {
        if (!client->charCurseList().contains(i))
            chars_taken_cursed.append("-1");
        else
            chars_taken_cursed.append(chars_taken.value(i));
    }
    return chars_taken_cursed;
}

bool ServerContext::isMessageAllowed() const
{
    return m_message_floodguard.isMessageAllowed();
}

void ServerContext::startMessageFloodguard(int f_duration)
{
    m_message_floodguard.start(f_duration);
}

QHostAddress ServerContext::parseToIPv4(QHostAddress f_remote_ip)
{
    bool l_ok;
    QHostAddress l_remote_ip = f_remote_ip;
    QHostAddress l_temp_remote_ip = QHostAddress(f_remote_ip.toIPv4Address(&l_ok));
    if (l_ok) {
        l_remote_ip = l_temp_remote_ip;
    }
    return l_remote_ip;
}

PlayerStateObserver *ServerContext::playerStateObserver()
{
    return &m_player_state_observer;
}

akashi::ArupBroadcaster *ServerContext::arupBroadcaster()
{
    return m_arup_broadcaster;
}

akashi::CommandRegistry *ServerContext::commandRegistry()
{
    return m_command_registry;
}

akashi::ConsoleMenu *ServerContext::consoleMenu()
{
    return m_console_menu;
}

akashi::PermissionRegistry *ServerContext::permissionRegistry()
{
    return m_permission_registry;
}

akashi::TextFilterRegistry *ServerContext::textFilterRegistry()
{
    return m_text_filter_registry;
}

akashi::RuleRegistry *ServerContext::ruleRegistry()
{
    return m_rule_registry;
}

akashi::AuthThrottle *ServerContext::authThrottle()
{
    return m_auth_throttle;
}

akashi::AuthenticatorRegistry *ServerContext::authenticatorRegistry()
{
    return m_authenticator_registry;
}

void ServerContext::reloadSettings()
{
    // Rules are deliberately not re-applied here: a reload must not undo
    // rule changes made at runtime. Rule edits in areas.json need a restart.
    m_config_store->reload();
}

void ServerContext::reloadTextData()
{
    m_text_data.gimps = akashi::config::loadTextFile(configPath("text/gimp.txt"));
    m_text_data.filters = akashi::config::loadTextFile(configPath("text/filter.txt"));
    m_text_data.compiled_filters.clear();
    for (const QString &l_pattern : std::as_const(m_text_data.filters))
        m_text_data.compiled_filters.append(QRegularExpression(l_pattern, QRegularExpression::CaseInsensitiveOption));
    m_text_data.cdns = akashi::config::loadTextFile(configPath("text/cdns.txt"));
    if (m_text_data.cdns.isEmpty())
        m_text_data.cdns = QStringList{"cdn.discord.com"};
    m_text_data.praises = akashi::config::loadTextFile(configPath("text/praise.txt"));
    m_text_data.reprimands = akashi::config::loadTextFile(configPath("text/reprimands.txt"));
    m_text_data.magic_8ball = akashi::config::loadTextFile(configPath("text/8ball.txt"));
    m_medieval_parser = std::make_unique<MedievalParser>(configPath("text/autorp.json"));
}

void ServerContext::reloadMusicFloor()
{
    auto l_music = akashi::config::loadMusicList(configPath("music.json"));
    akashi::Floor &l_default_floor = m_world->defaultFloor();
    l_default_floor.music_ordered = l_music.ordered;
    l_default_floor.approved_cdns = m_text_data.cdns;
    l_default_floor.music_songs.clear();
    for (auto it = l_music.songs.constBegin(); it != l_music.songs.constEnd(); ++it) {
        l_default_floor.music_songs.insert(it.key(), {it.key(), it.value().first, it.value().second});
    }
    const QVector<akashi::Area *> l_areas = m_world->areas();
    for (akashi::Area *l_area : l_areas) {
        l_area->jukebox()->setFloorCatalog(&l_default_floor);
    }
}

void ServerContext::reloadBanLists()
{
    m_ipban_list = akashi::config::loadIpRangeBans(configPath("ipbans.json"));
    m_banned_asns = akashi::config::loadBannedAsns(configPath("ipbans.json"));
    if (QFile::exists("storage/GeoLite2-ASN.mmdb")) {
        m_asn_reader.open("storage/GeoLite2-ASN.mmdb");
    }
}

void ServerContext::reloadAclRoles()
{
    acl_roles_handler->loadFile(configPath("acl_roles.json"));
}

void ServerContext::reloadAuthLimits()
{
    m_auth_throttle->setLimits(m_server_settings->max_login_attempts(),
                               m_server_settings->login_lockout_seconds());
}

void ServerContext::warnOnAuthSystemChange()
{
    if (configuredAuthSystemId() != m_active_auth_system_id) {
        qCWarning(akashiServer).noquote() << QStringLiteral("The auth setting now names \"%1\", but auth system changes take effect on restart; the server keeps running \"%2\".").arg(configuredAuthSystemId(), m_active_auth_system_id);
    }
}

void ServerContext::publishConfigReloaded()
{
    publishEvent(akashi::ConfigReloadedEvent::id, {});
}

void ServerContext::broadcast(const akashi::Packet &packet, int area_index)
{
    akashi::Area *l_bc_area = m_world->areaById(area_index);
    if (!l_bc_area) {
        return;
    }
    QVector<int> l_client_ids = l_bc_area->players();
    for (const int l_client_id : qAsConst(l_client_ids)) {
        akashi::ClientSession *l_client = clientById(l_client_id);
        if (l_client) {
            l_client->sendPacket(packet);
        }
    }
}

void ServerContext::broadcastIc(const QStringList &f_fields, int f_area_id, const std::function<QStringList(akashi::ClientSession *)> &f_viewer_fields)
{
    const QVector<akashi::ClientSession *> l_client_list = m_player_directory.clients();
    for (akashi::ClientSession *l_client : l_client_list) {
        if (l_client->areaId() != f_area_id) {
            continue;
        }
        const QStringList l_fields = f_viewer_fields ? f_viewer_fields(l_client) : f_fields;
        l_client->sendPacket(akashi::Packet("MS", l_fields));
    }
}

void ServerContext::kickClients(const QList<akashi::ClientSession *> &f_targets, const QString &f_target_ipid, const QString &f_reason, const QString &f_moderator)
{
    if (f_targets.isEmpty()) {
        return;
    }
    for (akashi::ClientSession *l_client : f_targets) {
        l_client->sendPacket("KK", {f_reason});
        l_client->closeSocket();
    }
    m_log_service->log({.type = akashi::log_type::Kick,
                        .message = f_reason,
                        .moderator = f_moderator,
                        .target_ipid = f_target_ipid});

    const akashi::KickIssuedEvent l_event{
        .moderator = f_moderator,
        .target_ipid = f_target_ipid,
        .reason = f_reason};
    publishEvent(akashi::KickIssuedEvent::id, akashi::eventToMap(l_event));
}

ServerContext::BanResult ServerContext::banPlayers(const QString &f_ipid, long long f_duration_secs, const QString &f_reason, const QString &f_moderator, const QString &f_perma_text, const std::function<QString(int f_ban_id, const QString &f_until)> &f_notice)
{
    DBManager::BanInfo l_ban;
    l_ban.ipid = f_ipid;
    l_ban.reason = f_reason;
    l_ban.moderator = f_moderator;
    l_ban.duration = f_duration_secs;
    l_ban.time = QDateTime::currentDateTime().toSecsSinceEpoch();

    QString l_duration_text = f_perma_text;
    if (l_ban.duration != -2) {
        l_duration_text = QDateTime::fromSecsSinceEpoch(l_ban.time).addSecs(l_ban.duration).toString("MM/dd/yyyy, hh:mm");
    }

    const QList<akashi::ClientSession *> l_targets = clientsByIpid(f_ipid);
    if (l_targets.isEmpty()) {
        // An offline ban is only the record; nobody to notify or drop.
        db_manager->addBan(l_ban);
        return {};
    }

    // One row per distinct hardware id, so every machine behind the IPID
    // is covered.
    QStringList l_banned_hwids;
    for (akashi::ClientSession *l_client : l_targets) {
        if (l_banned_hwids.contains(l_client->hwid())) {
            continue;
        }
        l_banned_hwids.append(l_client->hwid());
        l_ban.ip = l_client->remoteIp();
        l_ban.hdid = l_client->hwid();
        db_manager->addBan(l_ban);
    }

    const int l_ban_id = db_manager->banId(l_ban.ip);
    const QString l_notice = f_notice ? f_notice(l_ban_id, l_duration_text) : f_reason;
    for (akashi::ClientSession *l_client : l_targets) {
        l_client->sendPacket("KB", {l_notice});
        l_client->closeSocket();
    }

    m_log_service->log({.type = akashi::log_type::Ban,
                        .message = f_reason,
                        .moderator = f_moderator,
                        .target_ipid = f_ipid,
                        .duration = l_duration_text});

    const akashi::BanIssuedEvent l_event{
        .ban_id = l_ban_id,
        .moderator = f_moderator,
        .target_ipid = f_ipid,
        .duration = l_duration_text,
        .reason = f_reason};
    publishEvent(akashi::BanIssuedEvent::id, akashi::eventToMap(l_event));

    return {l_ban_id, static_cast<int>(l_targets.size())};
}

void ServerContext::broadcast(const akashi::Packet &packet)
{
    const QVector<akashi::ClientSession *> l_client_list = m_player_directory.clients();
    for (akashi::ClientSession *l_client : l_client_list) {
        l_client->sendPacket(packet);
    }
}

void ServerContext::broadcast(const akashi::Packet &packet, TARGET_TYPE target)
{
    const QVector<akashi::ClientSession *> l_client_list = m_player_directory.clients();
    for (akashi::ClientSession *l_client : l_client_list) {
        switch (target) {
        case TARGET_TYPE::MODCHAT:
            if (l_client->canPerform(akashi::permission::chat_moderator)) {
                l_client->sendPacket(packet);
            }
            break;
        case TARGET_TYPE::ADVERT:
            if (l_client->isAdvertEnabled()) {
                l_client->sendPacket(packet);
            }
            break;
        default:
            break;
        }
    }
}

void ServerContext::broadcast(const akashi::Packet &packet, const akashi::Packet &other_packet, TARGET_TYPE target)
{
    switch (target) {
    case TARGET_TYPE::AUTHENTICATED:
    {
        const QVector<akashi::ClientSession *> l_client_list = m_player_directory.clients();
        for (akashi::ClientSession *l_client : l_client_list) {
            if (l_client->isGlobalEnabled()) {
                if (l_client->isAuthenticated()) {
                    l_client->sendPacket(other_packet);
                }
                else {
                    l_client->sendPacket(packet);
                }
            }
        }
    }
    default:
        // Unimplemented, so not handled.
        break;
    }
}

void ServerContext::unicast(const akashi::Packet &f_packet, int f_client_id)
{
    akashi::ClientSession *l_client = clientById(f_client_id);
    if (l_client != nullptr) { // This should never happen, but safety first.
        l_client->sendPacket(f_packet);
        return;
    }
}

QList<akashi::ClientSession *> ServerContext::clientsByIpid(QString ipid)
{
    QList<akashi::ClientSession *> return_clients;
    const QVector<akashi::ClientSession *> l_client_list = m_player_directory.clients();
    for (akashi::ClientSession *l_client : l_client_list) {
        if (l_client->ipid() == ipid)
            return_clients.append(l_client);
    }
    return return_clients;
}

QList<akashi::ClientSession *> ServerContext::clientsByHwid(QString f_hwid)
{
    QList<akashi::ClientSession *> return_clients;
    const QVector<akashi::ClientSession *> l_client_list = m_player_directory.clients();
    for (akashi::ClientSession *l_client : l_client_list) {
        if (l_client->hwid() == f_hwid)
            return_clients.append(l_client);
    }
    return return_clients;
}

akashi::ClientSession *ServerContext::clientById(int id)
{
    return m_player_directory.clientById(id);
}

int ServerContext::playerCount()
{
    return m_player_count;
}

QStringList ServerContext::characters()
{
    return m_characters;
}

int ServerContext::characterCount()
{
    return m_characters.length();
}

QString ServerContext::characterById(int f_chr_id)
{
    QString l_chr;

    if (f_chr_id >= 0 && f_chr_id < m_characters.length()) {
        l_chr = m_characters.at(f_chr_id);
    }

    return l_chr;
}

int ServerContext::characterId(QString char_name)
{
    for (int i = 0; i < m_characters.length(); i++) {
        if (m_characters[i].toLower() == char_name.toLower()) {
            return i;
        }
    }

    return -1; // character does not exist
}

QVector<akashi::Area *> ServerContext::areas()
{
    return m_world->areas();
}

akashi::LogService *ServerContext::logService()
{
    return m_log_service;
}

void ServerContext::publishEvent(const QString &f_id, const QVariantMap &f_payload, int f_player_id)
{
    akashi::RuleContext l_ctx;
    l_ctx.player_id = f_player_id;
    l_ctx.area_id = -1;
    l_ctx.floor_id = -1;
    l_ctx.services = m_services;
    l_ctx.payload = f_payload;
    m_rule_registry->notifyObservers(f_id, l_ctx);
}

void ServerContext::flushModcallLog(const QString &f_area_name)
{
    if (loggingMode() == QStringLiteral("modcall") && m_text_writer) {
        m_text_writer->flushBuffer(f_area_name, m_log_service->recentEvents(f_area_name, 0));
    }
}

void ServerContext::refreshFloorClients(int f_floor_id)
{
    const QVector<akashi::ClientSession *> l_clients = clients();
    for (akashi::ClientSession *l_client : l_clients) {
        if (l_client->isJoined() && floorIdForArea(l_client->areaId()) == f_floor_id) {
            l_client->sendPacket(akashi::Packet("FA", l_client->floorAreaNames()));
            l_client->sendFullArup();
        }
    }
}

void ServerContext::onPluginAboutToUnload(const QString &f_plugin_id)
{
    const QStringList l_owned = m_rule_registry->actionsOwnedBy(f_plugin_id);
    const QSet<QString> l_actions(l_owned.begin(), l_owned.end());

    int l_removed = 0;
    for (akashi::Floor &l_floor : m_world->floors()) {
        l_removed += akashi::RuleRegistry::removeRules(f_plugin_id, l_actions, l_floor.before_rules, l_floor.after_rules, l_floor.transform_rules);
    }
    const QVector<akashi::Area *> l_sweep_areas = m_world->areas();
    for (akashi::Area *l_area : l_sweep_areas) {
        l_removed += akashi::RuleRegistry::removeRules(f_plugin_id, l_actions, l_area->beforeRules(), l_area->afterRules(), l_area->transformRules());
    }
    if (l_removed > 0) {
        qCInfo(akashiPlugins) << "Removed" << l_removed << "rule(s) that belonged to plugin" << f_plugin_id;
    }
}

// The rules an area or floor carries that belong in the config file: the
// ones config put there and the ones added by command.

// One area's settings and savable rules, in the shape areas.json reads.

QStringList ServerContext::musicList()
{
    return m_world->defaultFloor().music_ordered;
}

QStringList ServerContext::backgrounds()
{
    return m_backgrounds;
}

DBManager *ServerContext::databaseManager()
{
    return db_manager;
}

akashi::FileSystemService *ServerContext::fileSystem()
{
    return m_filesystem;
}

akashi::ServiceRegistry *ServerContext::services()
{
    return m_services;
}

std::shared_ptr<akashi::PacketService> ServerContext::packets()
{
    return m_packets;
}

akashi::ACLRolesHandler *ServerContext::aclRolesHandler()
{
    return acl_roles_handler;
}

void ServerContext::removeClient(akashi::ClientSession *f_client)
{
    akashi::PlayerDisconnectedEvent l_event;
    l_event.client_id = f_client->clientId();
    l_event.was_joined = f_client->isJoined();
    l_event.ipid = f_client->ipid();
    l_event.hwid = f_client->hwid();
    l_event.char_name = f_client->characterName();
    l_event.ooc_name = f_client->oocName();

    const bool l_was_joined = f_client->isJoined();
    if (l_was_joined) {
        decreasePlayerCount();
    }
    m_player_directory.removeClient(f_client->clientId());
    // Withdraw the client's presence while it is alive, then delete it - the
    // destructor never does the real teardown.
    f_client->leave();
    if (l_was_joined) {
        for (akashi::PlayerState *l_player : f_client->players) {
            m_player_state_observer.unregisterPlayer(l_player);
        }
    }
    f_client->deleteLater();

    // The player is fully gone; plugins clean up whatever they keyed on them.
    publishEvent(akashi::PlayerDisconnectedEvent::id, akashi::eventToMap(l_event), l_event.client_id);
}

void ServerContext::increasePlayerCount()
{
    m_player_count++;
    Q_EMIT playerCountUpdated(m_player_count);
}

void ServerContext::decreasePlayerCount()
{
    m_player_count--;
    Q_EMIT playerCountUpdated(m_player_count);
}

bool ServerContext::isIPBanned(QHostAddress f_remote_IP)
{
    for (const QString &l_ipban : qAsConst(m_ipban_list)) {
        if (f_remote_IP.isInSubnet(QHostAddress::parseSubnet(l_ipban))) {
            return true;
        }
    }

    // The address is banned when the network it belongs to is owned by a banned ASN.
    const quint32 l_asn = m_asn_reader.asnForAddress(f_remote_IP);
    return l_asn != 0 && m_banned_asns.contains(l_asn);
}

akashi::ConfigStore *ServerContext::configStore() { return m_config_store; }
ServerSettings *ServerContext::serverSettings() { return m_server_settings; }

QString ServerContext::configPath(const QString &f_file) const
{
    return m_config_store ? m_config_store->filePath(f_file) : "config/" + f_file;
}

QString ServerContext::serverNickname() const
{
    QString l_tag = m_server_settings->server_nickname();
    return l_tag.isEmpty() ? m_server_settings->server_name() : l_tag;
}

QUrl ServerContext::assetUrl() const
{
    QByteArray l_url = m_server_settings->asset_url().toUtf8();
    if (QUrl(l_url).isValid()) {
        return QUrl(l_url);
    }
    qWarning("asset_url is not a valid url!");
    return QUrl(nullptr);
}

AuthType ServerContext::authType() const
{
    const bool l_advanced = m_server_settings->auth().compare(QLatin1String("advanced"), Qt::CaseInsensitive) == 0;
    return l_advanced ? AuthType::ADVANCED : AuthType::SIMPLE;
}

QString ServerContext::loggingMode() const
{
    return m_server_settings->logging().toLower();
}

void ServerContext::setMotd(const QString &f_motd)
{
    m_server_settings->motd.set(f_motd);
}

QStringList ServerContext::gimpList() const { return m_text_data.gimps; }
QStringList ServerContext::cdnList() const { return m_text_data.cdns; }
QStringList ServerContext::praiseList() const { return m_text_data.praises; }
QStringList ServerContext::reprimandsList() const { return m_text_data.reprimands; }
QStringList ServerContext::magic8BallAnswers() const { return m_text_data.magic_8ball; }

QStringList ServerContext::diceFaces(const QString &f_name) const
{
    return m_text_data.dice_faces.value(f_name);
}

ServerContext::ServerContext(QObject *parent) :
    QObject(parent)
{
}

ServerContext::~ServerContext()
{
    shutdown();
}

void ServerContext::shutdown()
{
    if (m_stage == Stage::Stopped) {
        return;
    }
    setStage(Stage::Draining);
    if (m_plugin_manager) {
        m_plugin_manager->shutdownAll();
    } // Empty the roster first so no teardown broadcast writes to a neighbour
    // mid-destruction, then delete synchronously - a deleteLater posted here
    // never runs, because the event loop is already gone at this point.
    const QVector<akashi::ClientSession *> l_clients = m_player_directory.clients();
    m_player_directory.clear();
    qDeleteAll(l_clients);

    delete m_receiver;
    m_receiver = nullptr;
    // The registry holds the core systems, which read the settings and the
    // database; it goes before both.
    m_active_authenticator.reset();
    delete m_authenticator_registry;
    delete acl_roles_handler;
    delete db_manager;
    delete m_auth_throttle;
    delete m_command_registry;
    delete m_permission_registry;
    delete m_rule_registry;
    delete m_server_settings;
    setStage(Stage::Stopped);
}

ServerContext::Stage ServerContext::stage() const
{
    return m_stage;
}

void ServerContext::setStage(Stage f_stage)
{
    if (m_stage == f_stage) {
        return;
    }
    m_stage = f_stage;
    Q_EMIT stageChanged(m_stage);
}

// --- The world and its client-facing wrappers ---

akashi::World *ServerContext::world() const
{
    return m_world;
}

akashi::Area *ServerContext::areaById(int f_area_id)
{
    return m_world->areaById(f_area_id);
}

QString ServerContext::areaName(int f_area_id)
{
    return m_world->areaName(f_area_id);
}

QStringList ServerContext::areaNames()
{
    return m_world->areaNames();
}

int ServerContext::areaCount()
{
    return m_world->areaCount();
}

int ServerContext::floorCount() const
{
    return m_world->floorCount();
}

const akashi::Floor *ServerContext::floorById(int f_floor_id) const
{
    return const_cast<const akashi::World *>(m_world) ? static_cast<const akashi::World *>(m_world)->floorById(f_floor_id) : nullptr;
}

akashi::Floor *ServerContext::floorById(int f_floor_id)
{
    return m_world->floorById(f_floor_id);
}

const akashi::Floor *ServerContext::floorByName(const QString &f_name) const
{
    return m_world->floorByName(f_name);
}

int ServerContext::floorIdForArea(int f_area_id) const
{
    return m_world->floorIdForArea(f_area_id);
}

QStringList ServerContext::floorNames() const
{
    return m_world->floorNames();
}

void ServerContext::applyConfigRules()
{
    m_world->applyConfigRules(QFileInfo(configPath("areas.json")).absoluteFilePath());
}

std::optional<QString> ServerContext::saveWorld()
{
    return m_world->save(QFileInfo(configPath("areas.json")).absoluteFilePath());
}

std::optional<QString> ServerContext::saveFloor(int f_floor_id)
{
    const akashi::Floor *l_floor = m_world->floorById(f_floor_id);
    if (!l_floor) {
        return QStringLiteral("There is no floor with that ID.");
    }
    const auto l_file_name = akashi::FileSystemService::sanitizedFileName(QString(l_floor->name).replace(' ', '_'));
    if (!l_file_name) {
        return QStringLiteral("The floor's name does not work as a file name.");
    }
    return m_world->saveFloor(f_floor_id, QFileInfo(configPath("floors/" + *l_file_name + ".json")).absoluteFilePath());
}

std::optional<QString> ServerContext::loadFloor(const QString &f_name)
{
    const auto l_file_name = akashi::FileSystemService::sanitizedFileName(QString(f_name).replace(' ', '_'));
    if (!l_file_name) {
        return QStringLiteral("That does not look like a floor name.");
    }
    const QString l_path = QFileInfo(configPath("floors/" + *l_file_name + ".json")).absoluteFilePath();
    if (!QFile::exists(l_path)) {
        return QStringLiteral("There is no saved floor named ") + f_name + QStringLiteral(".");
    }

    // Everyone on the floor keeps their place by area name where possible.
    struct Placement
    {
        akashi::ClientSession *client;
        QString area_name;
    };
    QVector<Placement> l_placements;
    const akashi::Floor *l_existing = m_world->floorByName(f_name);
    if (l_existing) {
        const QVector<akashi::ClientSession *> l_clients = clients();
        for (akashi::ClientSession *l_client : l_clients) {
            if (l_client->isJoined() && m_world->floorIdForArea(l_client->areaId()) == l_existing->id) {
                l_placements.append({l_client, m_world->areaName(l_client->areaId())});
            }
        }
    }

    int l_floor_id = -1;
    QVector<int> l_mapping;
    if (auto l_error = m_world->loadFloorFile(f_name, l_path, l_floor_id, l_mapping)) {
        return l_error;
    }
    applyAreaMapping(l_mapping);

    const akashi::Floor *l_floor = m_world->floorById(l_floor_id);
    for (const Placement &l_placement : qAsConst(l_placements)) {
        int l_area_id = l_floor->area_ids.first();
        for (int l_candidate : qAsConst(l_floor->area_ids)) {
            if (m_world->areaName(l_candidate) == l_placement.area_name) {
                l_area_id = l_candidate;
            }
        }
        akashi::ClientSession *l_client = l_placement.client;
        l_client->setAreaId(l_area_id);
        m_world->areaById(l_area_id)->addClient(characterId(l_client->character()), l_client->clientId());
        l_client->runAfterRule(akashi::AreaEvents::PlayerJoined,
                               {{QStringLiteral("from_area"), -1}, {QStringLiteral("from_floor"), -1}});
    }
    for (int l_area_id : qAsConst(l_floor->area_ids)) {
        updateCharsTaken(m_world->areaById(l_area_id));
    }
    return std::nullopt;
}

std::optional<QString> ServerContext::reloadWorld()
{
    // Pick up the file's current content, then refuse before any teardown
    // if it holds no usable world.
    m_config_store->reload();
    const QStringList l_groups = m_areas_ini->childGroups();
    const bool l_has_area = std::any_of(l_groups.begin(), l_groups.end(), [](const QString &f_raw) {
        bool l_is_area = false;
        f_raw.split(":").first().toInt(&l_is_area);
        return l_is_area;
    });
    if (!l_has_area) {
        return QStringLiteral("areas.json defines no areas; the world stays as it is.");
    }

    // Everyone keeps their place by area name where possible.
    struct Placement
    {
        akashi::ClientSession *client;
        QString area_name;
    };
    QVector<Placement> l_placements;
    const QVector<akashi::ClientSession *> l_clients = clients();
    for (akashi::ClientSession *l_client : l_clients) {
        if (l_client->isJoined()) {
            l_placements.append({l_client, m_world->areaName(l_client->areaId())});
        }
    }

    m_arup_broadcaster->clear();
    m_world->rebuild(QFileInfo(configPath("areas.json")).absoluteFilePath());

    for (const Placement &l_placement : qAsConst(l_placements)) {
        int l_area_id = m_world->areaNames().indexOf(l_placement.area_name);
        if (l_area_id < 0) {
            l_area_id = 0;
        }
        akashi::ClientSession *l_client = l_placement.client;
        l_client->setAreaId(l_area_id);
        m_world->areaById(l_area_id)->addClient(characterId(l_client->character()), l_client->clientId());
        // The join delivery catches the client up on the rebuilt area.
        l_client->runAfterRule(akashi::AreaEvents::PlayerJoined,
                               {{QStringLiteral("from_area"), -1}, {QStringLiteral("from_floor"), -1}});
    }
    const QVector<akashi::Area *> l_areas = m_world->areas();
    for (akashi::Area *l_area : l_areas) {
        updateCharsTaken(l_area);
    }
    return std::nullopt;
}

int ServerContext::createArea(const QString &f_name, int f_floor_id)
{
    const int l_area_id = m_world->createArea(f_name, f_floor_id);
    if (l_area_id >= 0) {
        refreshFloorClients(f_floor_id);
    }
    return l_area_id;
}

int ServerContext::createFloor(const QString &f_name)
{
    return m_world->createFloor(f_name);
}

bool ServerContext::renameArea(int f_area_id, const QString &f_name)
{
    if (!m_world->renameArea(f_area_id, f_name)) {
        return false;
    }
    refreshFloorClients(m_world->floorIdForArea(f_area_id));
    return true;
}

bool ServerContext::renameFloor(int f_floor_id, const QString &f_name)
{
    if (!m_world->renameFloor(f_floor_id, f_name)) {
        return false;
    }
    refreshFloorClients(f_floor_id);
    return true;
}

std::optional<QString> ServerContext::removeArea(int f_area_id)
{
    const int l_floor_id = m_world->floorIdForArea(f_area_id);
    QVector<int> l_mapping;
    if (auto l_error = m_world->removeArea(f_area_id, l_mapping)) {
        return l_error;
    }
    applyAreaMapping(l_mapping);
    refreshFloorClients(l_floor_id);
    return std::nullopt;
}

std::optional<QString> ServerContext::removeFloor(int f_floor_id)
{
    QVector<int> l_mapping;
    if (auto l_error = m_world->removeFloor(f_floor_id, l_mapping)) {
        return l_error;
    }
    m_arup_broadcaster->removeFloor(f_floor_id);
    applyAreaMapping(l_mapping);
    for (int i = 0; i < m_world->floorCount(); ++i) {
        refreshFloorClients(i);
    }
    return std::nullopt;
}

void ServerContext::applyAreaMapping(const QVector<int> &f_mapping)
{
    const QVector<akashi::ClientSession *> l_clients = clients();
    for (akashi::ClientSession *l_client : l_clients) {
        const int l_old = l_client->areaId();
        if (l_old >= 0 && l_old < f_mapping.size() && f_mapping[l_old] >= 0 && f_mapping[l_old] != l_old) {
            l_client->setAreaId(f_mapping[l_old]);
        }
    }
}

void ServerContext::onAreaBuilt(akashi::Area *f_area)
{
    connect(f_area->jukebox(), &akashi::Jukebox::musicListChanged, this, std::bind_front(&ServerContext::onJukeboxMusicListChanged, this, f_area->id(), f_area));
    connect(f_area->jukebox(), &akashi::Jukebox::songStarted, this, std::bind_front(&ServerContext::onJukeboxSongStarted, this, f_area));
    if (m_arup_broadcaster) {
        m_arup_broadcaster->addArea(f_area, f_area->floorId());
    }
}

void ServerContext::onJukeboxMusicListChanged(int f_area_id, akashi::Area *f_area)
{
    broadcast(akashi::Packet("FM", f_area->jukebox()->resolvedList()), f_area_id);
}

void ServerContext::onJukeboxSongStarted(akashi::Area *f_area, const akashi::JukeboxSong &f_song)
{
    broadcast(akashi::Packet("MC", {f_song.real_name, QString::number(-1)}), f_area->id());

    // A jukebox-driven change has no acting player; the music_changed
    // after rules still run so reactions fire, under the null player.
    akashi::RuleContext l_ctx;
    l_ctx.player_id = -1;
    l_ctx.area_id = f_area->id();
    l_ctx.floor_id = f_area->floorId();
    l_ctx.services = services();
    l_ctx.payload = {{QStringLiteral("song"), f_song.real_name}, {QStringLiteral("source"), QStringLiteral("jukebox")}};
    const akashi::Floor *l_floor = floorById(f_area->floorId());
    akashi::RuleRegistry::runAfter(akashi::AreaEvents::MusicChanged, l_ctx,
                                   f_area->afterRules(),
                                   l_floor ? l_floor->after_rules : QVector<akashi::AfterRuleEntry>{});
    // The committed fact reaches the global observers here too.
    m_rule_registry->notifyObservers(akashi::AreaEvents::MusicChanged, l_ctx);
}

void ServerContext::onAreaAboutToBeRemoved(akashi::Area *f_area)
{
    if (m_arup_broadcaster) {
        m_arup_broadcaster->removeArea(f_area);
    }
}
