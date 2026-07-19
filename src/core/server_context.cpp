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
#include "akashi/sanctions.h"
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
#include "core/moderation_service.h"
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
#include <QSet>
#include <QTextStream>

#include <algorithm>
#include <utility>

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

    if (l_areas->childGroups().size() < 1) {
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

    // The roles file lives at permissions.json now; a leftover acl_roles
    // file (INI or JSON) migrates itself once and stays behind as a backup.
    m_config_store->migrateConfigFile(QStringLiteral("acl_roles"), QStringLiteral("json"), QStringLiteral("permissions"));

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
    m_command_registry->applyExtensions(configPath("command_extensions.json"),
                                        std::bind_front(&akashi::PermissionRegistry::isRegistered, m_permission_registry));
    // Config grants apply after the plugins start, like the extensions,
    // so grants of plugin-registered permissions land too - and the role
    // quarantine runs once every permission that will exist does.
    applyConfigGrants();
    applyEveryoneBaseline();
    validateRoles();
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
    const int l_length = std::max(m_server_settings->pass_min_length(), 8);
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
    acl_roles_handler->loadFile(configPath("permissions.json"));
    refreshCmPowers();

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
    m_services->registerService(std::shared_ptr<akashi::PlayerDirectory>(&m_player_directory, [](auto *) {}));
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
    m_services->registerService(std::make_shared<akashi::CoreModerationService>(db_manager, acl_roles_handler, this));
}

std::optional<QString> ServerContext::applyWordFilter(const QString &f_text) const
{
    QString l_result = f_text;
    for (const QRegularExpression &l_re : std::as_const(m_text_data.compiled_filters))
        l_result.replace(l_re, QStringLiteral("❌"));
    return l_result;
}

std::optional<QString> ServerContext::applyGimpFilter(const QString &f_text) const
{
    const auto &l_list = gimpList();
    // A present-but-empty gimp.txt leaves nothing to swap in; pass the real
    // text through rather than index an empty list. Returning nullopt here
    // would drop the message and silently mute the gimped player instead.
    if (l_list.isEmpty()) {
        return f_text;
    }
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
    m_command_registry->applyExtensions(configPath("command_extensions.json"),
                                        std::bind_front(&akashi::PermissionRegistry::isRegistered, m_permission_registry));
    applyConfigGrants();
    applyEveryoneBaseline();
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

    m_console_menu->registerAction(QStringLiteral("show stored sanctions"), std::bind_front(&ServerContext::showStoredSanctions, this));

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

void ServerContext::showStoredSanctions()
{
    auto *l_session = akashi::ConsoleMenu::activeSession();
    if (!l_session) {
        return;
    }
    const QList<DBManager::SanctionInfo> l_rows = db_manager->allSanctions();
    if (l_rows.isEmpty()) {
        l_session->printOut(QStringLiteral("No stored sanctions."));
        return;
    }
    for (const DBManager::SanctionInfo &l_row : l_rows) {
        const QString l_until = l_row.expires < 0
                                    ? QStringLiteral("until lifted by hand")
                                    : QStringLiteral("until ") + QDateTime::fromSecsSinceEpoch(l_row.expires).toString(QStringLiteral("yyyy-MM-dd hh:mm"));
        QString l_line = QStringLiteral("%1 | %2 | by %3 | %4").arg(l_row.ipid, l_row.sanction, l_row.moderator, l_until);
        if (!l_row.data.isEmpty()) {
            l_line += QStringLiteral(" | ") + l_row.data;
        }
        l_session->printOut(l_line);
    }
    l_session->printOut(QStringLiteral("Lift one with /lift_sanction <ipid> <sanction> in game."));
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

QList<akashi::ClientSession *> ServerContext::sessionsForIdentity(const QString &f_ipid, const QString &f_hwid) const
{
    QList<akashi::ClientSession *> l_sessions = clientsByIpid(f_ipid);
    if (!f_hwid.isEmpty()) {
        const QList<akashi::ClientSession *> l_by_hwid = clientsByHwid(f_hwid);
        for (akashi::ClientSession *l_client : l_by_hwid) {
            if (!l_sessions.contains(l_client)) {
                l_sessions.append(l_client);
            }
        }
    }
    return l_sessions;
}

void ServerContext::applySanctionToSession(akashi::ClientSession *f_client, const QString &f_sanction, const QString &f_data)
{
    f_client->setSanction(f_sanction, true);
    if (f_sanction == akashi::sanction::charcurse) {
        // The payload carries character names; a name the roster no
        // longer knows resolves to -1, which reads as "may go back to
        // character select" rather than pinning the person to nothing.
        f_client->clearCharCurse();
        const QStringList l_names = f_data.split(QLatin1Char(','));
        for (const QString &l_name : l_names) {
            f_client->addCharCurse(characterId(l_name));
        }
    }
}

QList<akashi::ClientSession *> ServerContext::applySanction(const QString &f_ipid, const QString &f_hwid, const QString &f_sanction, const QString &f_moderator, const std::optional<QDateTime> &f_until, const QString &f_data, akashi::ClientSession *f_exempt)
{
    // An untimed sanction holds until lifted by hand: the row stores -1
    // and any older scheduled lift must not cut it short.
    const qint64 l_expires = f_until.has_value() ? f_until->toSecsSinceEpoch() : -1;
    db_manager->upsertSanction({f_ipid, f_sanction, f_moderator,
                                QDateTime::currentSecsSinceEpoch(), l_expires, f_hwid, f_data});
    if (f_until.has_value()) {
        scheduleSanctionLift(f_ipid, f_sanction, *f_until);
    }
    else {
        m_scheduler->cancel(sanctionJobId(f_ipid, f_sanction));
    }
    QList<akashi::ClientSession *> l_sessions = sessionsForIdentity(f_ipid, f_hwid);
    l_sessions.removeAll(f_exempt);
    for (akashi::ClientSession *l_client : l_sessions) {
        applySanctionToSession(l_client, f_sanction, f_data);
    }
    return l_sessions;
}

QList<akashi::ClientSession *> ServerContext::removeSanction(const QString &f_ipid, const QString &f_sanction)
{
    // The stored row remembers the hardware id, so the lift reaches the
    // person's other windows even when only the IPID names them.
    const auto l_row = db_manager->sanctionRow(f_ipid, f_sanction);
    db_manager->removeSanction(f_ipid, f_sanction);
    m_scheduler->cancel(sanctionJobId(f_ipid, f_sanction));
    const QList<akashi::ClientSession *> l_sessions = sessionsForIdentity(f_ipid, l_row.has_value() ? l_row->hwid : QString());
    for (akashi::ClientSession *l_client : l_sessions) {
        l_client->setSanction(f_sanction, false);
        if (f_sanction == akashi::sanction::charcurse) {
            l_client->clearCharCurse();
        }
    }
    return l_sessions;
}

void ServerContext::applyTimedSanction(const QString &f_ipid, const QString &f_sanction, const QDateTime &f_until, const QString &f_moderator)
{
    const QList<akashi::ClientSession *> l_online = clientsByIpid(f_ipid);
    const QString l_hwid = l_online.isEmpty() ? QString() : l_online.first()->hwid();
    applySanction(f_ipid, l_hwid, f_sanction, f_moderator, f_until);
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
    const QList<akashi::ClientSession *> l_clients = removeSanction(f_ipid, f_sanction);
    for (akashi::ClientSession *l_client : l_clients) {
        l_client->sendServerMessage(QStringLiteral("Your \"%1\" sanction has expired.").arg(f_sanction));
    }
    qCInfo(akashiServer).noquote() << QStringLiteral("Sanction \"%1\" on %2 expired.").arg(f_sanction, f_ipid);
}

void ServerContext::armStoredSanctions()
{
    const QList<DBManager::SanctionInfo> l_stored = db_manager->allSanctions();
    int l_armed = 0;
    for (const DBManager::SanctionInfo &l_sanction : l_stored) {
        if (l_sanction.expires < 0) {
            continue;
        }
        scheduleSanctionLift(l_sanction.ipid, l_sanction.sanction, QDateTime::fromSecsSinceEpoch(l_sanction.expires));
        ++l_armed;
    }
    if (l_armed > 0) {
        qCInfo(akashiServer) << "Armed" << l_armed << "stored sanction lift(s).";
    }
}

void ServerContext::applyStoredSanctions(akashi::ClientSession *f_client)
{
    const QList<DBManager::SanctionInfo> l_active = db_manager->sanctionsForIdentity(f_client->ipid(), f_client->hwid(), QDateTime::currentSecsSinceEpoch());
    for (const DBManager::SanctionInfo &l_sanction : l_active) {
        applySanctionToSession(f_client, l_sanction.sanction, l_sanction.data);
    }
}

ExitCode ServerContext::startListening()
{
    // The --check-config dry run wants every load and validation this
    // assembly performs; only the socket and the advertiser are skipped.
    if (QCoreApplication::arguments().contains(QStringLiteral("--check-config"))) {
        qCInfo(akashiServer) << "--check-config: skipping the socket and advertiser.";
    }
    else {
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

        const akashi::TrustedProxyList l_trusted_proxies =
            akashi::WebSocketTransport::parseTrustedProxies(m_server_settings->trusted_proxies());
        // The configured auth id stands in for the active one, which
        // resolves only after the plugins start - a config naming an
        // unloadable system stops the server anyway.
        QStringList l_vocabulary = akashi::serverFeatures();
        l_vocabulary.append(QStringLiteral("auth_") + configuredAuthSystemId());
        m_receiver = new akashi::WebSocketReceiver(bind_addr, m_port, l_trusted_proxies, l_vocabulary, this);
        connect(m_receiver, &akashi::ClientReceiver::inboundClient, this, &ServerContext::inboundClient);
        if (!m_receiver->start()) {
            qCCritical(akashiServer) << "Server error:" << m_receiver->lastError();
            return ExitCode::PortUnavailable;
        }
        qCInfo(akashiServer) << "Server listening on" << m_receiver->port();

        // Construct modern advertiser if enabled in config
        server_publisher = new ServerPublisher(m_receiver->port(), &m_player_count, m_server_settings, this);
    }

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

    reloadBanLists();

    // Text data must reload before the music floor (cdns feed into it).
    connect(m_config_store, &akashi::ConfigStore::configReloaded, this, &ServerContext::reloadTextData);
    connect(m_config_store, &akashi::ConfigStore::configReloaded, this, &ServerContext::reloadMusicFloor);
    connect(m_config_store, &akashi::ConfigStore::configReloaded, this, &ServerContext::reloadBanLists);
    connect(m_config_store, &akashi::ConfigStore::configReloaded, m_log_service, &akashi::LogService::reloadTemplates);
    connect(m_config_store, &akashi::ConfigStore::configReloaded, this, &ServerContext::reloadAclRoles);
    // Offers follow the roles on a settings reload: unlike rules, no
    // runtime edit can be resurrected by reapplying areas.json grants,
    // and permissions.json already re-reads here - the two halves of the
    // permission surface must never go stale through different doors.
    connect(m_config_store, &akashi::ConfigStore::configReloaded, this, &ServerContext::applyConfigGrants);
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
    l_register_perm(akashi::permission::area_cm, QStringLiteral("Case Manager"), QStringLiteral("area"));
    l_register_perm(akashi::permission::timer_global, QStringLiteral("Global Timer"), QStringLiteral("area"));
    l_register_perm(akashi::permission::modify_evidence, QStringLiteral("Modify Evidence"), QStringLiteral("area"));
    l_register_perm(akashi::permission::motd, QStringLiteral("MOTD"), QStringLiteral("administration"));
    l_register_perm(akashi::permission::announcer, QStringLiteral("Announcer"), QStringLiteral("moderation"));
    l_register_perm(akashi::permission::chat_moderator, QStringLiteral("Chat Moderator"), QStringLiteral("moderation"));
    l_register_perm(akashi::permission::sanction_mute, QStringLiteral("Mute"), QStringLiteral("moderation"));
    l_register_perm(akashi::permission::sanction_ooc_mute, QStringLiteral("OOC Mute"), QStringLiteral("moderation"));
    l_register_perm(akashi::permission::sanction_block_dj, QStringLiteral("Block DJ"), QStringLiteral("moderation"));
    l_register_perm(akashi::permission::sanction_block_wtce, QStringLiteral("Block Judge Controls"), QStringLiteral("moderation"));
    l_register_perm(akashi::permission::sanction_gimp, QStringLiteral("Gimp"), QStringLiteral("moderation"));
    l_register_perm(akashi::permission::sanction_disemvowel, QStringLiteral("Disemvowel"), QStringLiteral("moderation"));
    l_register_perm(akashi::permission::sanction_shake, QStringLiteral("Shake"), QStringLiteral("moderation"));
    l_register_perm(akashi::permission::sanction_medieval, QStringLiteral("Medieval"), QStringLiteral("moderation"));
    l_register_perm(akashi::permission::sanction_charcurse, QStringLiteral("Charcurse"), QStringLiteral("moderation"));
    l_register_perm(akashi::permission::area_uncm, QStringLiteral("Remove CM"), QStringLiteral("moderation"));
    l_register_perm(akashi::permission::save_testimony, QStringLiteral("Save Testimony"), QStringLiteral("area"));
    l_register_perm(akashi::permission::force_charselect, QStringLiteral("Force Charselect"), QStringLiteral("moderation"));
    l_register_perm(akashi::permission::area_enter, QStringLiteral("Enter Locked Areas"), QStringLiteral("moderation"));
    l_register_perm(akashi::permission::ignore_background_list, QStringLiteral("Ignore BG List"), QStringLiteral("area"));
    l_register_perm(akashi::permission::send_notice, QStringLiteral("Send Notice"), QStringLiteral("moderation"));
    l_register_perm(akashi::permission::music_jukebox, QStringLiteral("Jukebox"), QStringLiteral("area"));
    l_register_perm(akashi::permission::modify_rules, QStringLiteral("Modify Rules"), QStringLiteral("area"));
    l_register_perm(akashi::permission::modify_floors, QStringLiteral("Modify Floors"), QStringLiteral("administration"));
    l_register_perm(akashi::permission::super, QStringLiteral("Super"), QStringLiteral("administration"));
    l_register_perm(akashi::permission::user, QStringLiteral("User"), QStringLiteral("lifecycle"));
    l_register_perm(akashi::permission::see_ipids, QStringLiteral("See IPIDs"), QStringLiteral("moderation"));
    l_register_perm(akashi::permission::receive_modcalls, QStringLiteral("Receive Modcalls"), QStringLiteral("moderation"));
    l_register_perm(akashi::permission::see_staff_presence, QStringLiteral("See Staff Presence"), QStringLiteral("moderation"));
    l_register_perm(akashi::permission::see_real_names, QStringLiteral("See Real Names"), QStringLiteral("moderation"));
    l_register_perm(akashi::permission::change_locked_background, QStringLiteral("Change Locked Background"), QStringLiteral("area"));
    l_register_perm(akashi::permission::ic_chat, QStringLiteral("IC Chat"), QStringLiteral("speech"));
    l_register_perm(akashi::permission::ooc_chat, QStringLiteral("OOC Chat"), QStringLiteral("speech"));
    l_register_perm(akashi::permission::change_music, QStringLiteral("Change Music"), QStringLiteral("area"));
    l_register_perm(akashi::permission::use_wtce, QStringLiteral("Use Judge Controls"), QStringLiteral("area"));
    l_register_perm(akashi::permission::sanction_immune, QStringLiteral("Sanction Immunity"), QStringLiteral("moderation"));
    l_register_perm(akashi::permission::music_play, QStringLiteral("Play Music"), QStringLiteral("music"));
    l_register_perm(akashi::permission::music_play_ambience, QStringLiteral("Play Ambience"), QStringLiteral("music"));
    l_register_perm(akashi::permission::music_currentmusic, QStringLiteral("Show Current Music"), QStringLiteral("music"));
    l_register_perm(akashi::permission::messaging_g, QStringLiteral("Global Chat"), QStringLiteral("messaging"));
    l_register_perm(akashi::permission::messaging_need, QStringLiteral("Send Adverts"), QStringLiteral("messaging"));
    l_register_perm(akashi::permission::messaging_pm, QStringLiteral("Private Messages"), QStringLiteral("messaging"));
    l_register_perm(akashi::permission::messaging_a, QStringLiteral("Message Owned Area"), QStringLiteral("messaging"));
    l_register_perm(akashi::permission::messaging_s, QStringLiteral("Message Owned Areas"), QStringLiteral("messaging"));
    l_register_perm(akashi::permission::roleplay_coinflip, QStringLiteral("Coin Flip"), QStringLiteral("roleplay"));
    l_register_perm(akashi::permission::roleplay_roll, QStringLiteral("Roll Dice"), QStringLiteral("roleplay"));
    l_register_perm(akashi::permission::roleplay_rolla, QStringLiteral("Roll Named Die"), QStringLiteral("roleplay"));
    l_register_perm(akashi::permission::roleplay_rollp, QStringLiteral("Roll Privately"), QStringLiteral("roleplay"));
    l_register_perm(akashi::permission::roleplay_notecard, QStringLiteral("Write Notecard"), QStringLiteral("roleplay"));
    l_register_perm(akashi::permission::roleplay_notecard_clear, QStringLiteral("Clear Notecard"), QStringLiteral("roleplay"));
    l_register_perm(akashi::permission::roleplay_8ball, QStringLiteral("Magic 8-Ball"), QStringLiteral("roleplay"));
    l_register_perm(akashi::permission::casing_doc, QStringLiteral("Document"), QStringLiteral("casing"));
    l_register_perm(akashi::permission::casing_cleardoc, QStringLiteral("Clear Document"), QStringLiteral("casing"));
    l_register_perm(akashi::permission::casing_testimony, QStringLiteral("View Testimony"), QStringLiteral("casing"));
    l_register_perm(akashi::permission::area_background, QStringLiteral("Change Background"), QStringLiteral("area"));
    l_register_perm(akashi::permission::area_status, QStringLiteral("Change Status"), QStringLiteral("area"));
    l_register_perm(akashi::permission::area_webfiles, QStringLiteral("List Webfiles"), QStringLiteral("area"));
    l_register_perm(akashi::permission::area_area, QStringLiteral("Move To Area"), QStringLiteral("area"));
    l_register_perm(akashi::permission::area_floor, QStringLiteral("Move To Floor"), QStringLiteral("area"));
    l_register_perm(akashi::permission::area_floors, QStringLiteral("List Floors"), QStringLiteral("area"));
    l_register_perm(akashi::permission::area_getarea, QStringLiteral("List Area Clients"), QStringLiteral("area"));
    l_register_perm(akashi::permission::area_getareas, QStringLiteral("List All Clients"), QStringLiteral("area"));
    l_register_perm(akashi::permission::characters_switch, QStringLiteral("Switch Character"), QStringLiteral("characters"));
    l_register_perm(akashi::permission::characters_randomchar, QStringLiteral("Random Character"), QStringLiteral("characters"));
    l_register_perm(akashi::permission::characters_charselect, QStringLiteral("Character Select"), QStringLiteral("characters"));
    l_register_perm(akashi::permission::characters_pos, QStringLiteral("Change Position"), QStringLiteral("characters"));
    l_register_perm(akashi::permission::messaging_afk, QStringLiteral("AFK"), QStringLiteral("messaging"));
    l_register_perm(akashi::permission::messaging_firstperson, QStringLiteral("First-Person Mode"), QStringLiteral("messaging"));
    l_register_perm(akashi::permission::messaging_toggleglobal, QStringLiteral("Toggle Global Chat"), QStringLiteral("messaging"));
    l_register_perm(akashi::permission::messaging_mutepm, QStringLiteral("Mute PMs"), QStringLiteral("messaging"));
    l_register_perm(akashi::permission::messaging_toggleadverts, QStringLiteral("Toggle Adverts"), QStringLiteral("messaging"));
    l_register_perm(akashi::permission::info_help, QStringLiteral("Help"), QStringLiteral("info"));
    l_register_perm(akashi::permission::info_commands, QStringLiteral("List Commands"), QStringLiteral("info"));
    l_register_perm(akashi::permission::info_why, QStringLiteral("Explain Permissions"), QStringLiteral("info"));
    l_register_perm(akashi::permission::info_motd, QStringLiteral("View MOTD"), QStringLiteral("info"));
    l_register_perm(akashi::permission::info_mods, QStringLiteral("List Moderators"), QStringLiteral("info"));
    l_register_perm(akashi::permission::info_about, QStringLiteral("About"), QStringLiteral("info"));
    l_register_perm(akashi::permission::info_rules, QStringLiteral("View Rules"), QStringLiteral("info"));
    l_register_perm(akashi::permission::info_ruleactions, QStringLiteral("List Rule Actions"), QStringLiteral("info"));
    // The case-manager powers, registered from the one list so a @cm group
    // may reference any of them (music.jukebox is already registered above,
    // so the guard skips it).
    for (const QString &l_cm_power : akashi::permission::defaultCmPowers()) {
        if (!m_permission_registry->isRegistered(l_cm_power)) {
            l_register_perm(l_cm_power, QStringLiteral("CM: ") + l_cm_power.section(QLatin1Char('.'), 1), QStringLiteral("cm"));
        }
    }

    // The user baseline: finishing the join handshake is the grant, a
    // declared server-scope everyone-offer instead of a resolver rule.
    // The one core-owned everyone-grant left - it covers only the
    // login-and-account door and packet dispatch, so authenticating
    // always works. Everything else, the speech-and-play baseline
    // included, comes from the everyone section (or the stock default)
    // through applyEveryoneBaseline, and a whitelist server may close
    // all of it.
    m_permission_registry->addGrant({akashi::permission::user, akashi::Audience::everyone(), akashi::GrantScope::Server, QStringLiteral("core")});

    // The block sanctions stay masks - the model's one subtraction takes
    // exactly one act away, whatever grants it, staff grants included.
    m_permission_registry->registerSanctionMask(akashi::sanction::muted, akashi::permission::ic_chat, QStringLiteral("core"));
    m_permission_registry->registerSanctionMask(akashi::sanction::ooc_muted, akashi::permission::ooc_chat, QStringLiteral("core"));
    m_permission_registry->registerSanctionMask(akashi::sanction::dj_blocked, akashi::permission::change_music, QStringLiteral("core"));
    m_permission_registry->registerSanctionMask(akashi::sanction::wtce_blocked, akashi::permission::use_wtce, QStringLiteral("core"));

    // The built-in grant sources feeding the resolution union.
    m_permission_registry->registerGrantSource(
        QStringLiteral("area_owner"), std::bind_front(&ServerContext::offerAreaOwner, this), QStringLiteral("core"));
    m_permission_registry->registerGrantSource(
        QStringLiteral("role_grants"), std::bind_front(&ServerContext::offerRoleGrants, this), QStringLiteral("core"));
    m_permission_registry->registerGrantSource(
        QStringLiteral("simple_auth"), &ServerContext::offerSimpleAuth, QStringLiteral("core"));
    m_permission_registry->registerGrantSource(
        QStringLiteral("lock_state"), std::bind_front(&ServerContext::offerLockState, this), QStringLiteral("core"));
    m_permission_registry->registerGrantSource(
        QStringLiteral("place_offers"), std::bind_front(&ServerContext::offerPlaceGrants, this), QStringLiteral("core"));

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

// Ownership is the walk-in CM grant: the owner list is live area state,
// so the offer resolves fresh on every query and vanishes with /uncm.
QList<akashi::Grant> ServerContext::offerAreaOwner(const akashi::PermissionQuery &f_query)
{
    // Ownership confers the CM status (area.cm) and the configured CM
    // powers (the @cm group, or the stock bundle) - both area-scoped and
    // resolved live, so they vanish the instant /uncm empties the owners.
    if (f_query.permission != akashi::permission::area_cm && !m_cm_powers.contains(f_query.permission)) {
        return {};
    }
    akashi::Area *l_area = areaById(f_query.area_id);
    if (l_area && l_area->owners().contains(f_query.client_id)) {
        return {{f_query.permission, akashi::Audience::person(f_query.ipid), akashi::GrantScope::Area, QStringLiteral("core")}};
    }
    return {};
}

void ServerContext::refreshCmPowers()
{
    // The case-manager power set the area_owner source hands to owners: the
    // @cm group if the roles file declares one, otherwise the stock bundle.
    // area.cm (the status) is granted alongside and is never listed here.
    QStringList l_powers = acl_roles_handler->groupExists(QStringLiteral("@cm"))
                               ? acl_roles_handler->groupMembers(QStringLiteral("@cm"))
                               : akashi::permission::defaultCmPowers();
    m_cm_powers = QSet<QString>(l_powers.begin(), l_powers.end());
}

// The roles file: every permission the actor's role holds, worn through
// authentication. A role holding super matches any query - the one
// declared wildcard, a member of the union rather than a short-circuit.
QList<akashi::Grant> ServerContext::offerRoleGrants(const akashi::PermissionQuery &f_query)
{
    if (!f_query.is_authenticated || f_query.acl_role_id.isEmpty()) {
        return {};
    }
    const akashi::ACLRole l_role = acl_roles_handler->roleById(f_query.acl_role_id);
    if (l_role.permissions().contains(akashi::permission::super) || l_role.permissions().contains(f_query.permission)) {
        return {{f_query.permission, akashi::Audience::role(f_query.acl_role_id), akashi::GrantScope::Server, QStringLiteral("roles")}};
    }
    return {};
}

// The simple-auth blanket as a declared grant instead of a hidden hatch:
// any logged-in client under the password system holds every permission.
QList<akashi::Grant> ServerContext::offerSimpleAuth(const akashi::PermissionQuery &f_query)
{
    if (f_query.is_authenticated && f_query.auth_type == QStringLiteral("simple")) {
        return {{f_query.permission, akashi::Audience::everyone(), akashi::GrantScope::Server, QStringLiteral("core")}};
    }
    return {};
}

// The moderation and administration hammers that never go on an
// everyone-shaped offer, whatever the scope - a config typo must not
// hand a room the mute or the IPID radar.
static const QSet<QString> &dangerousPermissions()
{
    static const QSet<QString> s_dangerous{
        akashi::permission::super, akashi::permission::kick, akashi::permission::ban,
        akashi::permission::chat_moderator,
        akashi::permission::sanction_mute, akashi::permission::sanction_ooc_mute,
        akashi::permission::sanction_block_dj, akashi::permission::sanction_block_wtce,
        akashi::permission::sanction_gimp, akashi::permission::sanction_disemvowel,
        akashi::permission::sanction_shake, akashi::permission::sanction_medieval,
        akashi::permission::sanction_charcurse,
        akashi::permission::see_ipids, akashi::permission::see_real_names,
        akashi::permission::see_staff_presence, akashi::permission::receive_modcalls,
        akashi::permission::modify_users, akashi::permission::modify_floors,
        akashi::permission::modify_rules,
        akashi::permission::area_enter, akashi::permission::force_charselect,
        akashi::permission::sanction_immune, akashi::permission::area_uncm,
        akashi::permission::timer_global};
    return s_dangerous;
}

// Turns one declaration into grant entries: the @group reference or
// single permission expanded, the audience parsed, every name checked
// against the catalog - unknowns warn loudly and are skipped, so a typo
// never loads as policy.
static QVector<akashi::Grant> compileGrantDeclaration(const akashi::config::GrantDeclaration &f_declaration,
                                                      akashi::GrantScope f_scope, const QString &f_where,
                                                      const akashi::ACLRolesHandler &f_roles,
                                                      const akashi::PermissionRegistry &f_registry)
{
    akashi::Audience l_audience;
    if (f_declaration.audience.compare(QStringLiteral("everyone"), Qt::CaseInsensitive) == 0) {
        l_audience = akashi::Audience::everyone();
    }
    else if (f_declaration.audience.compare(QStringLiteral("participants"), Qt::CaseInsensitive) == 0) {
        l_audience = akashi::Audience::participants();
    }
    else if (f_declaration.audience.startsWith(QStringLiteral("role:"), Qt::CaseInsensitive)) {
        l_audience = akashi::Audience::role(f_declaration.audience.mid(5));
    }
    else {
        qCWarning(akashiConfig) << f_where << "grants" << f_declaration.permission << "to unknown audience" << f_declaration.audience << R"(- use "everyone", "participants" or "role:<id>". The grant was skipped.)";
        return {};
    }

    QStringList l_permissions;
    if (f_declaration.permission.startsWith(QLatin1Char('@'))) {
        if (!f_roles.groupExists(f_declaration.permission)) {
            qCWarning(akashiConfig) << f_where << "grants unknown group" << f_declaration.permission << "- the grant was skipped.";
            return {};
        }
        l_permissions = f_roles.groupMembers(f_declaration.permission);
    }
    else if (const auto l_alias = akashi::permission::legacyAliases().constFind(f_declaration.permission.toLower());
             l_alias != akashi::permission::legacyAliases().constEnd()) {
        qCWarning(akashiConfig) << f_where << "grants" << f_declaration.permission << "by its legacy name - now" << l_alias->join(QStringLiteral(", ")) << "- update the file.";
        l_permissions = *l_alias;
    }
    else {
        l_permissions.append(f_declaration.permission.toLower());
    }

    QVector<akashi::Grant> l_grants;
    for (const QString &l_permission : std::as_const(l_permissions)) {
        if (!f_registry.isRegistered(l_permission)) {
            qCWarning(akashiConfig) << f_where << "grants unknown permission" << l_permission << "- the grant was skipped.";
            continue;
        }
        // The guard rail: the hammers never ride an everyone-shaped offer
        // - a room can be handed the jukebox, never the mute. Checked on
        // the expanded set, so a group cannot smuggle a member past it.
        if ((l_audience.kind == akashi::AudienceKind::Everyone || l_audience.kind == akashi::AudienceKind::Participants) && dangerousPermissions().contains(l_permission)) {
            qCCritical(akashiConfig) << f_where << "tries to offer" << l_permission << "to everyone - moderation and administration powers are role grants. The grant was refused.";
            continue;
        }
        l_grants.append({l_permission, l_audience, f_scope, QStringLiteral("config")});
    }
    return l_grants;
}

void ServerContext::applyConfigGrants()
{
    // Config grants are replaced wholesale, like config rules: core and
    // runtime entries stay untouched.
    for (akashi::Floor &l_floor : m_world->floors()) {
        l_floor.grants.removeIf([](const akashi::Grant &g) { return g.owner == QStringLiteral("config"); });
    }
    const QVector<akashi::Area *> l_areas = areas();
    for (akashi::Area *l_area : l_areas) {
        l_area->grants().removeIf([](const akashi::Grant &g) { return g.owner == QStringLiteral("config"); });
    }

    const akashi::config::AreaGrantsConfig l_config = akashi::config::loadAreaGrants(QFileInfo(configPath("areas.json")).absoluteFilePath());

    for (auto it = l_config.floor_grants.constBegin(); it != l_config.floor_grants.constEnd(); ++it) {
        akashi::Floor *l_floor = m_world->floorByName(it.key());
        if (!l_floor) {
            qCWarning(akashiConfig) << "areas.json declares grants for unknown floor" << it.key();
            continue;
        }
        for (const akashi::config::GrantDeclaration &l_declaration : it.value()) {
            l_floor->grants += compileGrantDeclaration(l_declaration, akashi::GrantScope::Floor,
                                                       QStringLiteral("floor ") + it.key(), *acl_roles_handler, *m_permission_registry);
        }
    }

    for (auto it = l_config.area_grants.constBegin(); it != l_config.area_grants.constEnd(); ++it) {
        akashi::Area *l_area = areaById(it.key());
        if (!l_area) {
            qCWarning(akashiConfig) << "areas.json declares grants for unknown area" << it.key();
            continue;
        }
        for (const akashi::config::GrantDeclaration &l_declaration : it.value()) {
            l_area->grants() += compileGrantDeclaration(l_declaration, akashi::GrantScope::Area,
                                                        QStringLiteral("area ") + l_area->name(), *acl_roles_handler, *m_permission_registry);
        }
    }
}

void ServerContext::applyEveryoneBaseline()
{
    // The ordinary-operation baseline: the everyone section decides what a
    // joined person may do, or the stock default applies when no section
    // exists (a converted 1.9 file keeps stock behavior). Replaced
    // wholesale like config grants; the lifecycle floor stays core-owned
    // and untouchable from here.
    m_permission_registry->removeGrantsByOwner(QStringLiteral("baseline"));
    const QStringList l_names = acl_roles_handler->everyoneBaseline().value_or(akashi::permission::defaultBaseline());
    for (const QString &l_name : l_names) {
        if (dangerousPermissions().contains(l_name)) {
            qCCritical(akashiConfig) << "permissions: the everyone section tries to offer" << l_name << "- moderation and administration powers are role grants. The grant was refused.";
            continue;
        }
        if (!m_permission_registry->isRegistered(l_name)) {
            qCCritical(akashiConfig) << "permissions: the everyone section grants unknown permission" << l_name << "- skipped until the file is fixed.";
            continue;
        }
        m_permission_registry->addGrant({l_name, akashi::Audience::everyone(), akashi::GrantScope::Server, QStringLiteral("baseline")});
    }
}

void ServerContext::validateRoles()
{
    const QStringList l_role_ids = acl_roles_handler->roleIds();
    for (const QString &l_role_id : l_role_ids) {
        const QSet<QString> l_permissions = acl_roles_handler->roleById(l_role_id).permissions();
        QStringList l_unknown;
        for (const QString &l_permission : l_permissions) {
            if (!m_permission_registry->isRegistered(l_permission)) {
                l_unknown << l_permission;
            }
        }
        if (!l_unknown.isEmpty()) {
            l_unknown.sort();
            qCCritical(akashiConfig) << "permissions: role" << l_role_id << "grants unknown permission(s)" << l_unknown.join(QStringLiteral(", ")) << "- the role is quarantined and grants nothing until the file is fixed.";
            acl_roles_handler->removeRole(l_role_id);
        }
    }
}

void ServerContext::printCompiledOffers() const
{
    QTextStream l_out(stdout);
    const auto l_grant_row = [](const akashi::Grant &f_grant) -> QString {
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
    };
    const auto l_sorted_rows = [&l_grant_row](const auto &f_grants) {
        QStringList l_rows;
        for (const akashi::Grant &l_grant : f_grants)
            l_rows << l_grant_row(l_grant);
        l_rows.sort();
        return l_rows;
    };

    l_out << "== Compiled offers ==\n";
    l_out << "Roles:\n";
    const QStringList l_role_ids = acl_roles_handler->roleIds();
    for (const QString &l_role_id : l_role_ids) {
        QStringList l_permissions(acl_roles_handler->roleById(l_role_id).permissions().begin(),
                                  acl_roles_handler->roleById(l_role_id).permissions().end());
        l_permissions.sort();
        l_out << "  " << l_role_id << ": " << (l_permissions.isEmpty() ? QStringLiteral("(nothing)") : l_permissions.join(QStringLiteral(", "))) << "\n";
    }
    l_out << "Groups:\n";
    const QStringList l_group_names = acl_roles_handler->groupNames();
    for (const QString &l_group : l_group_names) {
        QStringList l_members = acl_roles_handler->groupMembers(l_group);
        l_members.sort();
        l_out << "  @" << l_group << ": " << l_members.join(QStringLiteral(", ")) << "\n";
    }
    l_out << "Server grants:\n";
    const QStringList l_server_rows = l_sorted_rows(m_permission_registry->serverGrants());
    for (const QString &l_row : l_server_rows)
        l_out << l_row << "\n";
    for (const akashi::Floor &l_floor : m_world->floors()) {
        if (l_floor.grants.isEmpty())
            continue;
        l_out << "Floor " << l_floor.name << ":\n";
        const QStringList l_rows = l_sorted_rows(l_floor.grants);
        for (const QString &l_row : l_rows)
            l_out << l_row << "\n";
    }
    const QVector<akashi::Area *> l_areas = areas();
    for (akashi::Area *l_area : l_areas) {
        if (l_area->grants().isEmpty())
            continue;
        l_out << "Area " << l_area->name() << ":\n";
        const QStringList l_rows = l_sorted_rows(l_area->grants());
        for (const QString &l_row : l_rows)
            l_out << l_row << "\n";
    }
    l_out.flush();
}

// The lock's standing offer: entering is the area.enter permission. A
// free or spectatable area offers it to everyone; a locked one offers it
// to its invited people - the invite list read live, each invite a
// person grant. Staff pass through their server-scope role grant.
QList<akashi::Grant> ServerContext::offerLockState(const akashi::PermissionQuery &f_query)
{
    if (f_query.permission != akashi::permission::area_enter) {
        return {};
    }
    akashi::Area *l_area = areaById(f_query.area_id);
    if (!l_area) {
        return {};
    }
    if (l_area->lockState() != akashi::Area::LockState::Locked) {
        return {{f_query.permission, akashi::Audience::everyone(), akashi::GrantScope::Area, QStringLiteral("core")}};
    }
    if (l_area->invited().contains(f_query.client_id)) {
        return {{f_query.permission, akashi::Audience::person(f_query.ipid), akashi::GrantScope::Area, QStringLiteral("core")}};
    }
    return {};
}

// The floor and area offer vectors - the "grants" sections the config
// compile writes, and the runtime home of person-scope area grants.
QList<akashi::Grant> ServerContext::offerPlaceGrants(const akashi::PermissionQuery &f_query)
{
    QList<akashi::Grant> l_offers;
    akashi::Area *l_area = areaById(f_query.area_id);
    if (!l_area) {
        return l_offers;
    }
    for (const akashi::Grant &l_grant : l_area->grants()) {
        if (l_grant.permission == f_query.permission && akashi::PermissionRegistry::audienceCovers(l_grant.audience, f_query)) {
            l_offers.append(l_grant);
        }
    }
    if (const akashi::Floor *l_floor = floorById(l_area->floorId())) {
        for (const akashi::Grant &l_grant : l_floor->grants) {
            if (l_grant.permission == f_query.permission && akashi::PermissionRegistry::audienceCovers(l_grant.audience, f_query)) {
                l_offers.append(l_grant);
            }
        }
    }
    return l_offers;
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
                                           ? akashi::PlayerDirectory::IdAssignment::Lowest
                                           : akashi::PlayerDirectory::IdAssignment::LastFreed);
}

QVector<akashi::ClientSession *> ServerContext::clients() const
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

    bool is_at_multiclient_limit = false;
    client->calculateIpid();
    auto ban = db_manager->isIPBanned(client->ipid());
    bool is_banned = ban.first;
    // O(1) same-origin tally from the directory: the new client is not
    // filed yet, so +1 counts its own arrival. This replaces an O(N) scan
    // over every connected client that made a join storm O(N^2).
    int multiclient_count = m_player_directory.sameIpCount(client->remoteIp()) + 1;

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
    // The roster index is the character id, so no per-name lookup is needed.
    const QList<int> l_taken_list = area->charactersTaken();
    const QSet<int> l_taken(l_taken_list.begin(), l_taken_list.end());
    QStringList chars_taken;
    chars_taken.reserve(m_characters.size());
    for (int i = 0; i < m_characters.size(); i++) {
        chars_taken.append(l_taken.contains(i) ? QStringLiteral("-1") : QStringLiteral("0"));
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

QStringList ServerContext::cursedCharsTaken(akashi::ClientSession *client, QStringList chars_taken) const
{
    QStringList chars_taken_cursed;
    for (int i = 0; i < chars_taken.size(); i++) {
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

QHostAddress ServerContext::parseToIPv4(QHostAddress f_remote_ip) const
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

akashi::ArupBroadcaster *ServerContext::arupBroadcaster() const
{
    return m_arup_broadcaster;
}

akashi::CommandRegistry *ServerContext::commandRegistry() const
{
    return m_command_registry;
}

akashi::ConsoleMenu *ServerContext::consoleMenu() const
{
    return m_console_menu;
}

akashi::PermissionRegistry *ServerContext::permissionRegistry() const
{
    return m_permission_registry;
}

akashi::TextFilterRegistry *ServerContext::textFilterRegistry() const
{
    return m_text_filter_registry;
}

akashi::RuleRegistry *ServerContext::ruleRegistry() const
{
    return m_rule_registry;
}

akashi::AuthThrottle *ServerContext::authThrottle() const
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
    // Parse the ranges once here; the ban check runs on every connection.
    m_ipban_subnets.clear();
    const QStringList l_ranges = akashi::config::loadIpRangeBans(configPath("ipbans.json"));
    for (const QString &l_range : l_ranges) {
        const auto l_subnet = QHostAddress::parseSubnet(l_range);
        if (l_subnet.first.isNull()) {
            qCWarning(akashiConfig) << "Ignoring IP range ban that does not parse as a subnet:" << l_range;
            continue;
        }
        m_ipban_subnets.append(l_subnet);
    }
    m_banned_asns = akashi::config::loadBannedAsns(configPath("ipbans.json"));
    if (QFile::exists("storage/GeoLite2-ASN.mmdb")) {
        m_asn_reader.open("storage/GeoLite2-ASN.mmdb");
    }
    else if (!m_banned_asns.isEmpty()) {
        qCWarning(akashiConfig) << "ipbans.json bans" << m_banned_asns.size() << "ASNs, but storage/GeoLite2-ASN.mmdb does not exist. ASN bans are inactive.";
    }
}

void ServerContext::reloadAclRoles()
{
    acl_roles_handler->loadFile(configPath("permissions.json"));
    refreshCmPowers();
    validateRoles();
    applyEveryoneBaseline();
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
    for (const int l_client_id : std::as_const(l_client_ids)) {
        akashi::ClientSession *l_client = clientById(l_client_id);
        if (l_client) {
            l_client->sendPacket(packet);
        }
    }
}

void ServerContext::broadcastIc(const QStringList &f_fields, int f_area_id, const std::function<QStringList(akashi::ClientSession *)> &f_viewer_fields)
{
    akashi::Area *l_area = m_world->areaById(f_area_id);
    if (!l_area) {
        return;
    }

    // Without a per-viewer transform every recipient gets the same packet,
    // so it encodes at most once.
    std::optional<akashi::Packet> l_packet = std::nullopt;

    const QVector<int> l_client_ids = l_area->players();
    for (const int l_client_id : l_client_ids) {
        akashi::ClientSession *l_client = clientById(l_client_id);
        if (!l_client) {
            continue;
        }
        if (f_viewer_fields) {
            l_client->sendPacket(akashi::Packet("MS", f_viewer_fields(l_client)));
            continue;
        }
        if (!l_packet) {
            l_packet = akashi::Packet("MS", f_fields);
        }
        l_client->sendPacket(*l_packet);
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
    l_ban.time = QDateTime::currentSecsSinceEpoch();

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
        // The privileged variant carries the sender's IPID, so it goes to
        // whoever may see IPIDs, not to "whoever is logged in".
        const QVector<akashi::ClientSession *> l_client_list = m_player_directory.clients();
        for (akashi::ClientSession *l_client : l_client_list) {
            if (l_client->isGlobalEnabled()) {
                if (l_client->canPerform(akashi::permission::see_ipids)) {
                    l_client->sendPacket(other_packet);
                }
                else {
                    l_client->sendPacket(packet);
                }
            }
        }
        break;
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

QList<akashi::ClientSession *> ServerContext::clientsByIpid(QString ipid) const
{
    QList<akashi::ClientSession *> return_clients;
    const QVector<akashi::ClientSession *> l_client_list = m_player_directory.clients();
    for (akashi::ClientSession *l_client : l_client_list) {
        if (l_client->ipid() == ipid)
            return_clients.append(l_client);
    }
    return return_clients;
}

QList<akashi::ClientSession *> ServerContext::clientsByHwid(QString f_hwid) const
{
    QList<akashi::ClientSession *> return_clients;
    const QVector<akashi::ClientSession *> l_client_list = m_player_directory.clients();
    for (akashi::ClientSession *l_client : l_client_list) {
        if (l_client->hwid() == f_hwid)
            return_clients.append(l_client);
    }
    return return_clients;
}

akashi::ClientSession *ServerContext::clientById(int id) const
{
    return m_player_directory.clientById(id);
}

int ServerContext::playerCount() const
{
    return m_player_count;
}

QStringList ServerContext::characters() const
{
    return m_characters;
}

int ServerContext::characterCount() const
{
    return m_characters.size();
}

QString ServerContext::characterById(int f_chr_id) const
{
    QString l_chr;

    if (f_chr_id >= 0 && f_chr_id < m_characters.size()) {
        l_chr = m_characters.at(f_chr_id);
    }

    return l_chr;
}

int ServerContext::characterId(QString char_name) const
{
    const QString l_needle = char_name.toLower();
    for (int i = 0; i < m_characters.size(); i++) {
        if (m_characters[i].toLower() == l_needle) {
            return i;
        }
    }

    return -1; // character does not exist
}

QVector<akashi::Area *> ServerContext::areas() const
{
    return m_world->areas();
}

akashi::LogService *ServerContext::logService() const
{
    return m_log_service;
}

void ServerContext::publishEvent(const QString &f_id, const QVariantMap &f_payload, int f_player_state_id, int f_client_session_id)
{
    akashi::RuleContext l_ctx;
    l_ctx.player_state_id = f_player_state_id;
    l_ctx.client_session_id = f_client_session_id;
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

QStringList ServerContext::musicList() const
{
    return m_world->defaultFloor().music_ordered;
}

QStringList ServerContext::backgrounds() const
{
    return m_backgrounds;
}

DBManager *ServerContext::databaseManager() const
{
    return db_manager;
}

akashi::FileSystemService *ServerContext::fileSystem() const
{
    return m_filesystem;
}

akashi::ServiceRegistry *ServerContext::services() const
{
    return m_services;
}

std::shared_ptr<akashi::PacketService> ServerContext::packets() const
{
    return m_packets;
}

akashi::ACLRolesHandler *ServerContext::aclRolesHandler() const
{
    return acl_roles_handler;
}

void ServerContext::removeClient(akashi::ClientSession *f_client)
{
    akashi::PlayerDisconnectedEvent l_event;
    l_event.client_session_id = f_client->clientId();
    l_event.player_state_id = f_client->active_player->id();
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
    publishEvent(akashi::PlayerDisconnectedEvent::id, akashi::eventToMap(l_event), l_event.player_state_id, l_event.client_session_id);
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
    for (const auto &l_subnet : std::as_const(m_ipban_subnets)) {
        if (f_remote_IP.isInSubnet(l_subnet.first, l_subnet.second)) {
            return true;
        }
    }

    // The address is banned when the network it belongs to is owned by a banned ASN.
    const quint32 l_asn = m_asn_reader.asnForAddress(f_remote_IP);
    return l_asn != 0 && m_banned_asns.contains(l_asn);
}

akashi::ConfigStore *ServerContext::configStore() const { return m_config_store; }
ServerSettings *ServerContext::serverSettings() const { return m_server_settings; }

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
    // Drop the observer's player pointers too: deleting the sessions below
    // destroys their PlayerState children, and the observer would otherwise be
    // left holding dangling references for the rest of its life.
    m_player_state_observer.clear();
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

akashi::Area *ServerContext::areaById(int f_area_id) const
{
    return m_world->areaById(f_area_id);
}

QString ServerContext::areaName(int f_area_id) const
{
    return m_world->areaName(f_area_id);
}

QStringList ServerContext::areaNames() const
{
    return m_world->areaNames();
}

int ServerContext::areaCount() const
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
    for (const Placement &l_placement : std::as_const(l_placements)) {
        int l_area_id = l_floor->area_ids.first();
        for (int l_candidate : std::as_const(l_floor->area_ids)) {
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
    for (int l_area_id : std::as_const(l_floor->area_ids)) {
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

    for (const Placement &l_placement : std::as_const(l_placements)) {
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

    // A jukebox-driven change has no acting slot or session; the
    // music_changed after rules still run so reactions fire, under the
    // null actor, and no actor keys are stamped into this payload.
    akashi::RuleContext l_ctx;
    l_ctx.player_state_id = -1;
    l_ctx.client_session_id = -1;
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
