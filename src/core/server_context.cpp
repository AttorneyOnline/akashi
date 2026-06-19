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

#include "akashi/database_service.h"
#include "akashi/filesystem_service.h"
#include "akashi/service_registry.h"
#include "core/event_bus.h"
#include "world/rule_registry.h"
#include "world/world.h"
#include "core/client_session.h"
#include "world/area.h"
#include "commands/area_commands.h"
#include "commands/authentication_commands.h"
#include "commands/casing_commands.h"
#include "commands/messaging_commands.h"
#include "commands/moderation_commands.h"
#include "commands/music_commands.h"
#include "commands/plugin_commands.h"
#include "commands/roleplay_commands.h"
#include "commands/rule_commands.h"
#include "akashi/config_store.h"
#include "akashi/setting_notifier.h"
#include "core/command_registry.h"
#include "core/config_loading.h"
#include "core/permission_registry.h"
#include "core/server_settings.h"
#include "core/db_manager.h"
#include "core/log_service.h"
#include "core/writer_text.h"
#include "world/floor.h"
#include "core/websocket_receiver.h"
#include "proto/packet.h"
#include "proto/packet_service.h"
#include "world/rule_actions.h"
#include "core/server_publisher.h"
#include "core/auth_throttle.h"
#include "core/text_filter_registry.h"
#include "world/arup_broadcaster.h"
#include "world/jukebox.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QRegularExpression>
#include "akashi/network_service.h"
#include "core/plugin_manager.h"
#include "core/thread_assert.h"
#include "proto/area_music.h"
#include "proto/chat.h"
#include "proto/handshake.h"
#include "proto/ic.h"
#include "proto/moderation.h"

#include <QTextStream>

static bool fileExists(const QFileInfo &f) { return f.exists() && f.isFile(); }
static bool dirExists(const QFileInfo &f) { return f.exists() && f.isDir(); }

static bool verifyServerConfig(akashi::ConfigStore *f_store, ServerSettings *f_settings)
{
    auto path = [&](const QString &f) { return f_store->filePath(f); };

    QStringList l_dirs{path(""), path("text/")};
    for (const QString &d : l_dirs) {
        if (!dirExists(QFileInfo(d))) {
            qCritical() << d + " does not exist!";
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
            qCritical() << f + " does not exist!";
            return false;
        }
    }

    if (l_areas->childGroups().length() < 1) {
        qCritical() << "areas.json is invalid!";
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
        qCritical() << "The server configuration is invalid!";
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
        l_busy_check = [this, l_max_players] { return playerCount() > l_max_players; };
    }
    m_database_service->scheduleMaintenance(
        m_server_settings->maintenance_time(),
        m_server_settings->maintenance_vacuum(),
        l_busy_check);
    connect(m_database_service, &akashi::DatabaseService::maintenanceTriggered,
            logService(), &akashi::LogService::runWriterMaintenance);
    setStage(Stage::ContentLoaded);

    const QString l_plugin_dir = QDir(m_config_store->rootPath()).absoluteFilePath(QStringLiteral("../plugins"));
    m_plugin_manager = new akashi::PluginManager(m_services, l_plugin_dir, this);
    m_services->registerService(std::shared_ptr<akashi::PluginManager>(m_plugin_manager, [](auto *) {}));
    // Applied rules hold functions built by plugin code; the server drops
    // them before the plugin's library leaves memory.
    connect(m_plugin_manager, &akashi::PluginManager::pluginAboutToUnload,
            this, &ServerContext::onPluginAboutToUnload);

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
    setStage(Stage::PluginsLoaded);
    setStage(Stage::Listening);
    setStage(Stage::Running);
    return ExitCode::Ok;
}


void ServerContext::buildCore()
{
    m_player_count = 0;

    m_server_settings = new ServerSettings(m_config_store);
    m_areas_ini = m_config_store->settings("areas");
    m_ambience_ini = m_config_store->settings("ambience");

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
                qCritical() << "dice.ini max mismatch!";
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
    m_event_bus = new akashi::EventBus;
    m_rule_registry = new akashi::RuleRegistry;
    akashi::registerCoreRuleActions(this, m_rule_registry);

    m_world = new akashi::World(m_rule_registry, m_services, m_filesystem, m_areas_ini, m_ambience_ini, this);
    connect(m_world, &akashi::World::areaBuilt, this, &ServerContext::onAreaBuilt);
    connect(m_world, &akashi::World::areaAboutToBeRemoved, this, &ServerContext::onAreaAboutToBeRemoved);

    m_text_filter_registry = new akashi::TextFilterRegistry;

    m_text_filter_registry->registerFilter(QStringLiteral("word-filter"), 100,
        [this](const QString &f_text) -> std::optional<QString> {
            QString l_result = f_text;
            for (const QRegularExpression &l_re : std::as_const(m_text_data.compiled_filters))
                l_result.replace(l_re, QStringLiteral("❌"));
            return l_result;
        }, true, QStringLiteral("core"));

    m_text_filter_registry->registerFilter(QStringLiteral("gimped"), 200,
        [this](const QString &) -> std::optional<QString> {
            const auto &l_list = gimpList();
            return l_list.at(QRandomGenerator::global()->bounded(l_list.size()));
        }, false, QStringLiteral("core"));

    m_text_filter_registry->registerFilter(QStringLiteral("shaken"), 400,
        [](const QString &f_text) -> std::optional<QString> {
            QStringList l_words = f_text.split(QStringLiteral(" "));
            std::shuffle(l_words.begin(), l_words.end(), *QRandomGenerator::global());
            return l_words.join(QStringLiteral(" "));
        }, false, QStringLiteral("core"));

    m_text_filter_registry->registerFilter(QStringLiteral("disemvoweled"), 500,
        [](const QString &f_text) -> std::optional<QString> {
            static const QRegularExpression l_vowels(QStringLiteral("[AEIOUaeiou]"));
            return QString(f_text).remove(l_vowels);
        }, false, QStringLiteral("core"));

    m_auth_throttle = new akashi::AuthThrottle(m_server_settings->max_login_attempts(),
                                               m_server_settings->login_lockout_seconds());

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
    m_services->registerService(std::shared_ptr<akashi::EventBus>(m_event_bus, [](auto *) {}));
    m_services->registerService(std::shared_ptr<akashi::TextFilterRegistry>(m_text_filter_registry, [](auto *) {}));
    m_services->registerService(std::shared_ptr<akashi::RuleRegistry>(m_rule_registry, [](auto *) {}));
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
        qCritical() << bind_ip << "is an invalid IP address to listen on! Server not starting, check your config.";
        return ExitCode::InvalidBindAddress;
    }

    QStringList l_vocabulary = akashi::serverFeatures();
        m_receiver = new akashi::WebSocketReceiver(bind_addr, m_port, l_vocabulary, this);
    connect(m_receiver, &akashi::ClientReceiver::inboundClient, this, &ServerContext::inboundClient);
    if (!m_receiver->start()) {
        qCritical() << "Server error:" << m_receiver->lastError();
        return ExitCode::PortUnavailable;
    }
    qInfo() << "Server listening on" << m_receiver->port();

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
    m_arup_broadcaster->setOwnerFormatter([this](int owner_id) -> QString {
        akashi::ClientSession *owner = clientById(owner_id);
        if (!owner) {
            return {};
        }
        return QStringLiteral("[") + QString::number(owner->clientId()) + QStringLiteral("] ") + owner->character();
    });
    connect(m_arup_broadcaster, &akashi::ArupBroadcaster::arupFloorBroadcast, this, [this](const akashi::Packet &packet, int floorId) {
        const akashi::Floor *l_floor = floorById(floorId);
        if (!l_floor) return;
        for (int l_aid : l_floor->area_ids) {
            broadcast(packet, l_aid);
        }
    });
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
    connect(m_config_store, &akashi::ConfigStore::configReloaded, this, [this]() {
        acl_roles_handler->loadFile(configPath("acl_roles.json"));
    });
    connect(m_config_store, &akashi::ConfigStore::configReloaded, this, [this]() {
        m_auth_throttle->setLimits(m_server_settings->max_login_attempts(),
                                   m_server_settings->login_lockout_seconds());
    });

    // Rate-Limiter for IC-Chat
    m_message_floodguard_timer = new QTimer(this);
    m_message_floodguard_timer->setSingleShot(true);
    connect(m_message_floodguard_timer, &QTimer::timeout, this, &ServerContext::allowMessage);

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

    // Register the built-in permission resolver chain.
    m_permission_registry->registerResolver(QStringLiteral("none_check"), 0,
        [](const akashi::PermissionQuery &q) -> akashi::PermissionVerdict {
            if (q.permission.isEmpty() || q.permission == akashi::permission::none) {
                return akashi::PermissionVerdict::Granted;
            }
            return akashi::PermissionVerdict::NoOpinion;
        }, QStringLiteral("core"));

    m_permission_registry->registerResolver(QStringLiteral("area_owner"), 100,
        [this](const akashi::PermissionQuery &q) -> akashi::PermissionVerdict {
            if (q.permission == akashi::permission::gamemaster) {
                akashi::Area *l_area = areaById(q.area_id);
                if (l_area && l_area->owners().contains(q.client_id)) {
                    return akashi::PermissionVerdict::Granted;
                }
            }
            return akashi::PermissionVerdict::NoOpinion;
        }, QStringLiteral("core"));

    m_permission_registry->registerResolver(QStringLiteral("authentication"), 200,
        [](const akashi::PermissionQuery &q) -> akashi::PermissionVerdict {
            if (!q.is_authenticated) {
                return akashi::PermissionVerdict::Denied;
            }
            if (q.auth_type == QStringLiteral("simple")) {
                return akashi::PermissionVerdict::Granted;
            }
            return akashi::PermissionVerdict::NoOpinion;
        }, QStringLiteral("core"));

    m_permission_registry->registerResolver(QStringLiteral("role_check"), 300,
        [this](const akashi::PermissionQuery &q) -> akashi::PermissionVerdict {
            const akashi::ACLRole l_role = acl_roles_handler->roleById(q.acl_role_id);
            if (l_role.canPerform(q.permission)) {
                return akashi::PermissionVerdict::Granted;
            }
            return akashi::PermissionVerdict::Denied;
        }, QStringLiteral("core"));

    akashi::commands::registerAreaCommands(*m_command_registry);
    akashi::commands::registerAuthenticationCommands(*m_command_registry);
    akashi::commands::registerModerationCommands(*m_command_registry);
    akashi::commands::registerCasingCommands(*m_command_registry);
    akashi::commands::registerMusicCommands(*m_command_registry);
    akashi::commands::registerRoleplayCommands(*m_command_registry);
    akashi::commands::registerMessagingCommands(*m_command_registry);
    akashi::commands::registerPluginCommands(*m_command_registry);
    akashi::commands::registerRuleCommands(*m_command_registry);
    m_command_registry->applyExtensions(configPath("command_extensions.json"));

    return ExitCode::Ok;
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
    connect(client, &akashi::ClientSession::transportClosed, this, [this, client](akashi::DisconnectKind f_kind) {
        // A lost connection may be a network problem the person comes right
        // back from: keep the client and its characters for the grace
        // period. A clean close, a client that never joined, or no grace
        // configured means removal right away.
        const int l_grace_seconds = m_server_settings->reconnect_grace();
        if (f_kind == akashi::DisconnectKind::Lost && l_grace_seconds > 0 && client->isJoined()) {
            client->waitForReconnect(l_grace_seconds);
            return;
        }
        removeClient(client);
    });
    connect(client, &akashi::ClientSession::reconnectTimedOut, this, [this, client] {
        removeClient(client);
    });

    // This is the infamous workaround for
    // tsuserver4. It should disable fantacrypt
    // completely in any client 2.4.3 or newer
    akashi::Packet decryptor("decryptor", {"NOENCRYPT"});
    client->sendPacket(decryptor);
    connect(client, &akashi::ClientSession::joined, this, &ServerContext::increasePlayerCount);
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
    return m_can_send_ic_messages;
}

void ServerContext::startMessageFloodguard(int f_duration)
{
    m_can_send_ic_messages = false;
    m_message_floodguard_timer->start(f_duration);
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

void ServerContext::broadcastIc(const QStringList &f_fields, int f_area_id)
{
    const akashi::Packet l_classic("MS", f_fields);
    const QVector<akashi::ClientSession *> l_client_list = m_player_directory.clients();
    for (akashi::ClientSession *l_client : l_client_list) {
        if (l_client->areaId() != f_area_id) {
            continue;
        }
        l_client->sendPacket(l_classic);
    }
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

akashi::EventBus *ServerContext::eventBus()
{
    return m_event_bus;
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
        l_removed += akashi::RuleRegistry::removeRules(f_plugin_id, l_actions, l_floor.before_rules, l_floor.after_rules);
    }
    const QVector<akashi::Area *> l_sweep_areas = m_world->areas();
    for (akashi::Area *l_area : l_sweep_areas) {
        l_removed += akashi::RuleRegistry::removeRules(f_plugin_id, l_actions, l_area->beforeRules(), l_area->afterRules());
    }
    if (l_removed > 0) {
        qInfo() << "Removed" << l_removed << "rule(s) that belonged to plugin" << f_plugin_id;
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

void ServerContext::allowMessage()
{
    m_can_send_ic_messages = true;
}


void ServerContext::removeClient(akashi::ClientSession *f_client)
{
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

void ServerContext::setAuthType(AuthType f_auth)
{
    m_server_settings->auth.set(f_auth == AuthType::ADVANCED ? QStringLiteral("advanced") : QStringLiteral("simple"));
}

QStringList ServerContext::gimpList() const { return m_text_data.gimps; }
QStringList ServerContext::filterList() const { return m_text_data.filters; }
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
    }    // Empty the roster first so no teardown broadcast writes to a neighbour
    // mid-destruction, then delete synchronously - a deleteLater posted here
    // never runs, because the event loop is already gone at this point.
    const QVector<akashi::ClientSession *> l_clients = m_player_directory.clients();
    m_player_directory.clear();
    qDeleteAll(l_clients);

    delete m_receiver;
    m_receiver = nullptr;
    delete acl_roles_handler;
    delete db_manager;
    delete m_auth_throttle;
    delete m_command_registry;
    delete m_permission_registry;
    delete m_event_bus;
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
    return m_world->renameFloor(f_floor_id, f_name);
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
    const int l_area_id = f_area->id();
    connect(f_area->jukebox(), &akashi::Jukebox::musicListChanged, this, [this, l_area_id, f_area]() {
        broadcast(akashi::Packet("FM", f_area->jukebox()->resolvedList()), l_area_id);
    });
    connect(f_area->jukebox(), &akashi::Jukebox::ambienceChanged, this, [this, l_area_id](const QString &f_song) {
        broadcast(akashi::Packet("MC", {f_song, QString::number(-1), serverNickname(), QString::number(1), QString::number(1)}), l_area_id);
    });
    connect(f_area->jukebox(), &akashi::Jukebox::songStarted, this, [this, l_area_id](const akashi::JukeboxSong &f_song) {
        broadcast(akashi::Packet("MC", {f_song.real_name, QString::number(-1)}), l_area_id);
    });
    if (m_arup_broadcaster) {
        m_arup_broadcaster->addArea(f_area, f_area->floorId());
    }
}

void ServerContext::onAreaAboutToBeRemoved(akashi::Area *f_area)
{
    if (m_arup_broadcaster) {
        m_arup_broadcaster->removeArea(f_area);
    }
}
