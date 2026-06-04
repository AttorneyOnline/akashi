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
#include "server.h"

#include "acl_roles_handler.h"
#include "akashi/database_service.h"
#include "akashi/filesystem_service.h"
#include "akashi/service_registry.h"
#include "core/event_bus.h"
#include "world/area_rules.h"
#include "aoclient.h"
#include "area_data.h"
#include "commands/area_commands.h"
#include "commands/authentication_commands.h"
#include "commands/casing_commands.h"
#include "commands/messaging_commands.h"
#include "commands/moderation_commands.h"
#include "commands/music_commands.h"
#include "commands/plugin_commands.h"
#include "commands/roleplay_commands.h"
#include "akashi/config_store.h"
#include "akashi/setting_notifier.h"
#include "core/command_registry.h"
#include "core/config_loading.h"
#include "core/permission_registry.h"
#include "core/server_settings.h"
#include "db_manager.h"
#include "core/log_service.h"
#include "core/writer_text.h"
#include "world/floor.h"
#include "network/network_socket.h"
#include "proto/handshake.h"
#include "proto/packet.h"
#include "proto/packet_service.h"
#include "serverpublisher.h"
#include "core/auth_throttle.h"
#include "core/text_filter_registry.h"
#include "world/arup_broadcaster.h"
#include "world/jukebox.h"

#include <QRandomGenerator>
#include <QRegularExpression>

Server::Server(int p_ws_port, akashi::ConfigStore *f_config_store, akashi::DatabaseService *f_database, akashi::ServiceRegistry *f_services, QObject *parent) :
    QObject(parent),
    m_config_store(f_config_store),
    m_port(p_ws_port),
    m_player_count(0)
{
    m_server_settings = new ServerSettings(f_config_store);
    m_areas_ini = f_config_store->settings("areas");
    m_ambience_ini = f_config_store->settings("ambience");

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

    QSettings &l_dice_ini = *f_config_store->settings("dice");
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

    m_services = f_services;
    if (m_services) {
        m_packets = m_services->resolve<akashi::PacketService>("akashi.packets");
    }
    m_filesystem = m_services->resolve<akashi::FileSystemService>("akashi.filesystem").get();
    db_manager = new DBManager(f_database->database());
    acl_roles_handler = new ACLRolesHandler(this);
    acl_roles_handler->loadFile(configPath("acl_roles.json"));

    m_permission_registry = new akashi::PermissionRegistry;
    m_command_registry = new akashi::CommandRegistry;
    m_event_bus = new akashi::EventBus;
    m_area_rule_registry = new akashi::AreaRuleRegistry;
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

    m_log_service = new akashi::LogService(f_config_store, m_server_settings->logbuffer(), this);

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
}

ExitCode Server::start()
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

    server = new QWebSocketServer("Akashi", QWebSocketServer::NonSecureMode, this);

    // The FL capability list doubles as the subprotocol vocabulary. The
    // echo picks the client's first offered token found here, so the
    // client leads with what it needs accepted; offering only tokens the
    // server does not speak fails the handshake, which IS the refusal.
    QStringList l_spoken;
    const QStringList l_features = akashi::serverFeatures();
    for (const QString &l_feature : l_features) {
        l_spoken.append(QStringLiteral("network_") + l_feature);
    }
    server->setSupportedSubprotocols(l_spoken);
    if (!server->listen(bind_addr, m_port)) {
        qCritical() << "Server error:" << server->errorString();
        return ExitCode::PortUnavailable;
    }
    connect(server, &QWebSocketServer::newConnection,
            this, &Server::clientConnected);
    qInfo() << "Server listening on" << server->serverPort();

    // Construct modern advertiser if enabled in config
    server_publisher = new ServerPublisher(server->serverPort(), &m_player_count, m_server_settings, this);

    m_characters = akashi::config::loadTextFile(configPath("characters.txt"));
    m_backgrounds = akashi::config::loadTextFile(configPath("backgrounds.txt"));

    // Seed the default floor's music catalog from music.json.
    auto l_music = akashi::config::loadMusicList(configPath("music.json"));
    m_default_floor.music_ordered = l_music.ordered;
    m_default_floor.approved_cdns = m_text_data.cdns;
    for (auto it = l_music.songs.constBegin(); it != l_music.songs.constEnd(); ++it) {
        m_default_floor.music_songs.insert(it.key(), {it.key(), it.value().first, it.value().second});
    }

    // Assembles the area list, reading an optional floor assignment per area.
    QStringList l_raw_names = m_areas_ini->childGroups();
    std::sort(l_raw_names.begin(), l_raw_names.end(), [](const QString &a, const QString &b) { return a.split(":")[0].toInt() < b.split(":")[0].toInt(); });

    QMap<QString, int> l_floor_name_to_id;
    QStringList l_area_floor_names;

    QSet<QString> l_ic_blocked_floors;
    for (const QString &l_raw : qAsConst(l_raw_names)) {
        QStringList l_parts = l_raw.split(":");
        l_parts.removeFirst();
        m_area_names.append(l_parts.join(":"));

        m_areas_ini->beginGroup(l_raw);
        QString l_floor_name = m_areas_ini->value("floor", "Default").toString();
        if (m_areas_ini->value("ic_allowed", "true").toString() == "false")
            l_ic_blocked_floors.insert(l_floor_name);
        m_areas_ini->endGroup();
        l_area_floor_names.append(l_floor_name);

        if (!l_floor_name_to_id.contains(l_floor_name)) {
            int l_id = l_floor_name_to_id.size();
            l_floor_name_to_id.insert(l_floor_name, l_id);
        }
    }

    m_floors.resize(l_floor_name_to_id.size());
    for (auto it = l_floor_name_to_id.constBegin(); it != l_floor_name_to_id.constEnd(); ++it) {
        m_floors[it.value()].id = it.value();
        m_floors[it.value()].name = it.key();
    }

    for (const QString &l_blocked : l_ic_blocked_floors) {
        int l_fid = l_floor_name_to_id.value(l_blocked, -1);
        if (l_fid >= 0) {
            m_area_rule_registry->registerFloorRule(
                QStringLiteral("no-ic"), akashi::AreaEvent::MessageSent, akashi::RulePhase::Before, l_fid,
                [name = l_blocked](const akashi::AreaEventDetails &) {
                    return akashi::RuleVerdict{false, "IC chat is disabled on the " + name + " floor."};
                }, QStringLiteral("core"));
        }
    }

    for (int i = 0; i < m_area_names.length(); i++) {
        int l_floor_id = l_floor_name_to_id.value(l_area_floor_names[i], 0);
        int l_x_on_floor = m_floors[l_floor_id].area_ids.size();
        m_floors[l_floor_id].area_ids.append(i);

        QString area_name = QString::number(i) + ":" + m_area_names[i];
        AreaData *l_area = new AreaData(area_name, i, l_floor_id, l_x_on_floor, m_areas_ini, m_ambience_ini);
        m_areas.insert(i, l_area);
        l_area->jukebox()->setFloorCatalog(&m_default_floor);

        connect(l_area->jukebox(), &akashi::Jukebox::musicListChanged, this, [this, i, l_area]() {
            broadcast(akashi::Packet("FM", l_area->jukebox()->resolvedList()), i);
        });
        connect(l_area->jukebox(), &akashi::Jukebox::ambienceChanged, this, [this, i](const QString &f_song) {
            broadcast(akashi::Packet("MC", {f_song, QString::number(-1), serverNickname(), QString::number(1), QString::number(1)}), i);
        });
        connect(l_area, &AreaData::userJoinedArea, this, [this, l_area](int f_area_index, int f_user_id) {
            Q_UNUSED(f_area_index)
            unicast(akashi::Packet("FM", l_area->jukebox()->resolvedList()), f_user_id);
            unicast(akashi::Packet("MC", {l_area->currentAmbience(), QString::number(-1), serverNickname(), QString::number(1), QString::number(1)}), f_user_id);
            unicast(akashi::Packet("MC", {l_area->currentMusic(), QString::number(-1), serverNickname(), QString::number(1)}), f_user_id);
        });
        connect(l_area->jukebox(), &akashi::Jukebox::songStarted, this, [this, i](const akashi::JukeboxSong &f_song) {
            broadcast(akashi::Packet("MC", {f_song.real_name, QString::number(-1)}), i);
        });
    }

    m_arup_broadcaster = new akashi::ArupBroadcaster(this);
    for (int i = 0; i < m_areas.size(); ++i) {
        m_arup_broadcaster->addArea(m_areas[i]->area(), m_areas[i]->area()->floorId());
    }
    m_arup_broadcaster->setOwnerFormatter([this](int owner_id) -> QString {
        AOClient *owner = clientById(owner_id);
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
    connect(m_arup_broadcaster, &akashi::ArupBroadcaster::arupUnicast, this, &Server::unicast);

    m_ipban_list = akashi::config::loadIpRangeBans(configPath("ipbans.json"));
    m_banned_asns = akashi::config::loadBannedAsns(configPath("ipbans.json"));
    if (QFile::exists("storage/GeoLite2-ASN.mmdb")) {
        m_asn_reader.open("storage/GeoLite2-ASN.mmdb");
    }

    // Text data must reload before the music floor (cdns feed into it).
    connect(m_config_store, &akashi::ConfigStore::configReloaded, this, &Server::reloadTextData);
    connect(m_config_store, &akashi::ConfigStore::configReloaded, this, &Server::reloadMusicFloor);
    connect(m_config_store, &akashi::ConfigStore::configReloaded, this, &Server::reloadBanLists);
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
    connect(m_message_floodguard_timer, &QTimer::timeout, this, &Server::allowMessage);

    m_player_directory.setCapacity(m_server_settings->max_players());
    applyIdAssignment();
    connect(m_server_settings->id_assignment.notifier(), &akashi::SettingNotifier::changed, this, &Server::applyIdAssignment);

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
                AreaData *l_area = areaById(q.area_id);
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
            const ACLRole l_role = acl_roles_handler->roleById(q.acl_role_id);
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
    m_command_registry->applyExtensions(configPath("command_extensions.json"));

    return ExitCode::Ok;
}

void Server::applyIdAssignment()
{
    m_player_directory.setIdAssignment(m_server_settings->id_assignment() == "lowest"
                                           ? PlayerDirectory::IdAssignment::Lowest
                                           : PlayerDirectory::IdAssignment::LastFreed);
}

QVector<AOClient *> Server::clients()
{
    return m_player_directory.clients();
}

void Server::clientConnected()
{
    QWebSocket *socket = server->nextPendingConnection();
    NetworkSocket *l_socket = new NetworkSocket(socket);

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
        AOClient *l_waiting_client = nullptr;
        const QVector<AOClient *> l_current_clients = m_player_directory.clients();
        for (AOClient *i_client : l_current_clients) {
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
    AOClient *client = new AOClient(this, l_socket, nullptr, user_id);

    int multiclient_count = 1;
    bool is_at_multiclient_limit = false;
    client->calculateIpid();
    auto ban = db_manager->isIPBanned(client->ipid());
    bool is_banned = ban.first;
    const QVector<AOClient *> l_connected_clients = m_player_directory.clients();
    for (AOClient *joined_client : l_connected_clients) {
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
        socket->sendTextMessage(ban_reason.serialize());
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
    connect(client, &AOClient::disconnected, this, [this, client](akashi::DisconnectKind f_kind) {
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
    connect(client, &AOClient::reconnectTimedOut, this, [this, client] {
        removeClient(client);
    });

    // This is the infamous workaround for
    // tsuserver4. It should disable fantacrypt
    // completely in any client 2.4.3 or newer
    akashi::Packet decryptor("decryptor", {"NOENCRYPT"});
    client->sendPacket(decryptor);
    connect(client, &AOClient::joined, this, &Server::increasePlayerCount);
}

void Server::updateCharsTaken(AreaData *area)
{
    QStringList chars_taken;
    for (const QString &cur_char : qAsConst(m_characters)) {
        chars_taken.append(area->charactersTaken().contains(characterId(cur_char))
                               ? QStringLiteral("-1")
                               : QStringLiteral("0"));
    }

    akashi::Packet response_cc("CharsCheck", chars_taken);

    const QVector<AOClient *> l_client_list = m_player_directory.clients();
    for (AOClient *l_client : l_client_list) {
        if (l_client->areaId() == area->index()) {
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

QStringList Server::cursedCharsTaken(AOClient *client, QStringList chars_taken)
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

bool Server::isMessageAllowed() const
{
    return m_can_send_ic_messages;
}

void Server::startMessageFloodguard(int f_duration)
{
    m_can_send_ic_messages = false;
    m_message_floodguard_timer->start(f_duration);
}

QHostAddress Server::parseToIPv4(QHostAddress f_remote_ip)
{
    bool l_ok;
    QHostAddress l_remote_ip = f_remote_ip;
    QHostAddress l_temp_remote_ip = QHostAddress(f_remote_ip.toIPv4Address(&l_ok));
    if (l_ok) {
        l_remote_ip = l_temp_remote_ip;
    }
    return l_remote_ip;
}

PlayerStateObserver *Server::playerStateObserver()
{
    return &m_player_state_observer;
}

akashi::ArupBroadcaster *Server::arupBroadcaster()
{
    return m_arup_broadcaster;
}

akashi::CommandRegistry *Server::commandRegistry()
{
    return m_command_registry;
}

akashi::PermissionRegistry *Server::permissionRegistry()
{
    return m_permission_registry;
}

akashi::TextFilterRegistry *Server::textFilterRegistry()
{
    return m_text_filter_registry;
}

akashi::AreaRuleRegistry *Server::areaRuleRegistry()
{
    return m_area_rule_registry;
}

akashi::AuthThrottle *Server::authThrottle()
{
    return m_auth_throttle;
}

void Server::reloadSettings()
{
    m_config_store->reload();
}

void Server::reloadTextData()
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

void Server::reloadMusicFloor()
{
    auto l_music = akashi::config::loadMusicList(configPath("music.json"));
    m_default_floor.music_ordered = l_music.ordered;
    m_default_floor.approved_cdns = m_text_data.cdns;
    m_default_floor.music_songs.clear();
    for (auto it = l_music.songs.constBegin(); it != l_music.songs.constEnd(); ++it) {
        m_default_floor.music_songs.insert(it.key(), {it.key(), it.value().first, it.value().second});
    }
    for (AreaData *l_area : qAsConst(m_areas)) {
        l_area->jukebox()->setFloorCatalog(&m_default_floor);
    }
}

void Server::reloadBanLists()
{
    m_ipban_list = akashi::config::loadIpRangeBans(configPath("ipbans.json"));
    m_banned_asns = akashi::config::loadBannedAsns(configPath("ipbans.json"));
    if (QFile::exists("storage/GeoLite2-ASN.mmdb")) {
        m_asn_reader.open("storage/GeoLite2-ASN.mmdb");
    }
}

void Server::broadcast(const akashi::Packet &packet, int area_index)
{
    QVector<int> l_client_ids = m_areas.value(area_index)->joinedIDs();
    for (const int l_client_id : qAsConst(l_client_ids)) {
        AOClient *l_client = clientById(l_client_id);
        if (l_client) {
            l_client->sendPacket(packet);
        }
    }
}

void Server::broadcast(const akashi::Packet &packet)
{
    const QVector<AOClient *> l_client_list = m_player_directory.clients();
    for (AOClient *l_client : l_client_list) {
        l_client->sendPacket(packet);
    }
}

void Server::broadcast(const akashi::Packet &packet, TARGET_TYPE target)
{
    const QVector<AOClient *> l_client_list = m_player_directory.clients();
    for (AOClient *l_client : l_client_list) {
        switch (target) {
        case TARGET_TYPE::MODCHAT:
            if (l_client->canPerform(ACLRole::MODCHAT)) {
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

void Server::broadcast(const akashi::Packet &packet, const akashi::Packet &other_packet, TARGET_TYPE target)
{
    switch (target) {
    case TARGET_TYPE::AUTHENTICATED:
    {
        const QVector<AOClient *> l_client_list = m_player_directory.clients();
        for (AOClient *l_client : l_client_list) {
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

void Server::unicast(const akashi::Packet &f_packet, int f_client_id)
{
    AOClient *l_client = clientById(f_client_id);
    if (l_client != nullptr) { // This should never happen, but safety first.
        l_client->sendPacket(f_packet);
        return;
    }
}

QList<AOClient *> Server::clientsByIpid(QString ipid)
{
    QList<AOClient *> return_clients;
    const QVector<AOClient *> l_client_list = m_player_directory.clients();
    for (AOClient *l_client : l_client_list) {
        if (l_client->ipid() == ipid)
            return_clients.append(l_client);
    }
    return return_clients;
}

QList<AOClient *> Server::clientsByHwid(QString f_hwid)
{
    QList<AOClient *> return_clients;
    const QVector<AOClient *> l_client_list = m_player_directory.clients();
    for (AOClient *l_client : l_client_list) {
        if (l_client->hwid() == f_hwid)
            return_clients.append(l_client);
    }
    return return_clients;
}

AOClient *Server::clientById(int id)
{
    return m_player_directory.clientById(id);
}

int Server::playerCount()
{
    return m_player_count;
}

QStringList Server::characters()
{
    return m_characters;
}

int Server::characterCount()
{
    return m_characters.length();
}

QString Server::characterById(int f_chr_id)
{
    QString l_chr;

    if (f_chr_id >= 0 && f_chr_id < m_characters.length()) {
        l_chr = m_characters.at(f_chr_id);
    }

    return l_chr;
}

int Server::characterId(QString char_name)
{
    for (int i = 0; i < m_characters.length(); i++) {
        if (m_characters[i].toLower() == char_name.toLower()) {
            return i;
        }
    }

    return -1; // character does not exist
}

QVector<AreaData *> Server::areas()
{
    return m_areas;
}

int Server::areaCount()
{
    return m_areas.length();
}

AreaData *Server::areaById(int f_area_id)
{
    AreaData *l_area = nullptr;

    if (f_area_id >= 0 && f_area_id < m_areas.length()) {
        l_area = m_areas.at(f_area_id);
    }

    return l_area;
}


akashi::LogService *Server::logService()
{
    return m_log_service;
}

akashi::EventBus *Server::eventBus()
{
    return m_event_bus;
}

void Server::flushModcallLog(const QString &f_area_name)
{
    if (loggingMode() == QStringLiteral("modcall") && m_text_writer) {
        m_text_writer->flushBuffer(f_area_name, m_log_service->recentEvents(f_area_name, 0));
    }
}

QStringList Server::areaNames()
{
    return m_area_names;
}

QString Server::areaName(int f_area_id)
{
    QString l_name;

    if (f_area_id >= 0 && f_area_id < m_area_names.length()) {
        l_name = m_area_names.at(f_area_id);
    }

    return l_name;
}

int Server::floorCount() const
{
    return m_floors.size();
}

const akashi::Floor *Server::floorById(int f_floor_id) const
{
    if (f_floor_id >= 0 && f_floor_id < m_floors.size())
        return &m_floors[f_floor_id];
    return nullptr;
}

const akashi::Floor *Server::floorByName(const QString &f_name) const
{
    for (const akashi::Floor &l_floor : m_floors) {
        if (l_floor.name.compare(f_name, Qt::CaseInsensitive) == 0)
            return &l_floor;
    }
    return nullptr;
}

int Server::floorIdForArea(int f_area_id) const
{
    if (f_area_id >= 0 && f_area_id < m_areas.size())
        return m_areas[f_area_id]->area()->floorId();
    return 0;
}

QStringList Server::floorNames() const
{
    QStringList l_names;
    for (const akashi::Floor &l_floor : m_floors)
        l_names.append(l_floor.name);
    return l_names;
}

QStringList Server::musicList()
{
    return m_default_floor.music_ordered;
}

QStringList Server::backgrounds()
{
    return m_backgrounds;
}

DBManager *Server::databaseManager()
{
    return db_manager;
}

akashi::FileSystemService *Server::fileSystem()
{
    return m_filesystem;
}

akashi::ServiceRegistry *Server::services()
{
    return m_services;
}

std::shared_ptr<akashi::PacketService> Server::packets()
{
    return m_packets;
}


ACLRolesHandler *Server::aclRolesHandler()
{
    return acl_roles_handler;
}

void Server::allowMessage()
{
    m_can_send_ic_messages = true;
}


void Server::removeClient(AOClient *f_client)
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
        for (akashi::PlayerState *l_player : f_client->players()) {
            m_player_state_observer.unregisterPlayer(l_player);
        }
    }
    f_client->deleteLater();
}

void Server::increasePlayerCount()
{
    m_player_count++;
    Q_EMIT playerCountUpdated(m_player_count);
}

void Server::decreasePlayerCount()
{
    m_player_count--;
    Q_EMIT playerCountUpdated(m_player_count);
}

bool Server::isIPBanned(QHostAddress f_remote_IP)
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

akashi::ConfigStore *Server::configStore() { return m_config_store; }
ServerSettings *Server::serverSettings() { return m_server_settings; }


QString Server::configPath(const QString &f_file) const
{
    return m_config_store ? m_config_store->filePath(f_file) : "config/" + f_file;
}

QString Server::serverNickname() const
{
    QString l_tag = m_server_settings->server_nickname();
    return l_tag.isEmpty() ? m_server_settings->server_name() : l_tag;
}

QUrl Server::assetUrl() const
{
    QByteArray l_url = m_server_settings->asset_url().toUtf8();
    if (QUrl(l_url).isValid()) {
        return QUrl(l_url);
    }
    qWarning("asset_url is not a valid url!");
    return QUrl(nullptr);
}

DataTypes::AuthType Server::authType() const
{
    QString l_auth = m_server_settings->auth().toUpper();
    return toDataType<DataTypes::AuthType>(l_auth);
}

QString Server::loggingMode() const
{
    return m_server_settings->logging().toLower();
}

void Server::setMotd(const QString &f_motd)
{
    m_server_settings->motd.set(f_motd);
}

void Server::setAuthType(DataTypes::AuthType f_auth)
{
    m_server_settings->auth.set(fromDataType<DataTypes::AuthType>(f_auth).toLower());
}

QStringList Server::gimpList() const { return m_text_data.gimps; }
QStringList Server::filterList() const { return m_text_data.filters; }
QStringList Server::cdnList() const { return m_text_data.cdns; }
QStringList Server::praiseList() const { return m_text_data.praises; }
QStringList Server::reprimandsList() const { return m_text_data.reprimands; }
QStringList Server::magic8BallAnswers() const { return m_text_data.magic_8ball; }

QStringList Server::diceFaces(const QString &f_name) const
{
    return m_text_data.dice_faces.value(f_name);
}

Server::~Server()
{
    // Empty the roster first so no teardown broadcast writes to a neighbour
    // mid-destruction, then delete synchronously - a deleteLater posted here
    // never runs, because the event loop is already gone at this point.
    const QVector<AOClient *> l_clients = m_player_directory.clients();
    m_player_directory.clear();
    qDeleteAll(l_clients);

    delete server;
    delete acl_roles_handler;
    delete db_manager;
    delete m_auth_throttle;
    delete m_command_registry;
    delete m_permission_registry;
    delete m_event_bus;
    delete m_area_rule_registry;
    delete m_server_settings;
}
