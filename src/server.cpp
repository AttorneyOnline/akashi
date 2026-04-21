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
#include "aoclient.h"
#include "area_data.h"
#include "command_extension.h"
#include "config_manager.h"
#include "db_manager.h"
#include "discord.h"
#include "logger/u_logger.h"
#include "music_manager.h"
#include "network/network_socket.h"
#include "proto/handshake.h"
#include "proto/packet.h"
#include "proto/packet_service.h"
#include "serverpublisher.h"

Server::Server(int p_ws_port, akashi::DatabaseService *f_database, akashi::ServiceRegistry *f_services, QObject *parent) :
    QObject(parent),
    m_port(p_ws_port),
    m_player_count(0)
{
    timer = new QTimer(this);

    m_services = f_services;
    if (m_services) {
        m_packets = m_services->resolve<akashi::PacketService>("akashi.packets");
    }
    m_filesystem = new akashi::FileSystemService();
    db_manager = new DBManager(f_database->database());
    medieval_parser = new MedievalParser;

    acl_roles_handler = new ACLRolesHandler(this);
    acl_roles_handler->loadFile(ConfigManager::path("acl_roles.json"));

    command_extension_collection = new CommandExtensionCollection;
    command_extension_collection->setCommandNameWhitelist(AOClient::COMMANDS.keys());
    command_extension_collection->loadFile(ConfigManager::path("command_extensions.json"));

    // We create it, even if its not used later on.
    discord = new Discord(this);

    logger = new ULogger(this);
    connect(this, &Server::logConnectionAttempt, logger, &ULogger::logConnectionAttempt);
}

ExitCode Server::start()
{
    QString bind_ip = ConfigManager::bindIP();
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

    // Checks if any Discord webhooks are enabled.
    handleDiscordIntegration();

    // Construct modern advertiser if enabled in config
    server_publisher = new ServerPublisher(server->serverPort(), &m_player_count, this);

    // Get characters from config file
    m_characters = ConfigManager::charlist();

    // Get backgrounds from config file
    m_backgrounds = ConfigManager::backgrounds();

    // Build our music manager.

    MusicList l_musiclist = ConfigManager::musiclist();
    music_manager = new MusicManager(ConfigManager::cdnList(), l_musiclist, ConfigManager::ordered_songs(), this);
    connect(music_manager, &MusicManager::sendFMPacket, this, &Server::unicast);
    connect(music_manager, &MusicManager::sendAreaFMPacket, this, QOverload<const akashi::Packet &, int>::of(&Server::broadcast));

    // Get musiclist from config file
    m_music_list = music_manager->rootMusiclist();

    // Assembles the area list
    m_area_names = ConfigManager::sanitizedAreaNames();
    for (int i = 0; i < m_area_names.length(); i++) {
        QString area_name = QString::number(i) + ":" + m_area_names[i];
        AreaData *l_area = new AreaData(area_name, i, music_manager);
        m_areas.insert(i, l_area);
        connect(l_area, &AreaData::sendAreaPacket, this, QOverload<const akashi::Packet &, int>::of(&Server::broadcast));
        connect(l_area, &AreaData::sendAreaPacketClient, this, &Server::unicast);
        connect(l_area, &AreaData::userJoinedArea, music_manager, &MusicManager::userJoinedArea);
        music_manager->registerArea(i);
    }

    // Loads the command help information. This is not stored inside the server.
    ConfigManager::loadCommandHelp();

    // Get IP bans
    m_ipban_list = ConfigManager::iprangeBans();
    m_banned_asns = ConfigManager::bannedAsns();
    if (QFile::exists("storage/GeoLite2-ASN.mmdb")) {
        m_asn_reader.open("storage/GeoLite2-ASN.mmdb");
    }

    // Rate-Limiter for IC-Chat
    m_message_floodguard_timer = new QTimer(this);
    m_message_floodguard_timer->setSingleShot(true);
    connect(m_message_floodguard_timer, &QTimer::timeout, this, &Server::allowMessage);

    // Prepare player IDs and reference hash.
    for (int i = ConfigManager::maxPlayers() - 1; i >= 0; i--) {
        m_available_ids.push(i);
        m_clients_ids.insert(i, nullptr);
    }

    return ExitCode::Ok;
}

QVector<AOClient *> Server::clients()
{
    return m_clients;
}

void Server::clientConnected()
{
    QWebSocket *socket = server->nextPendingConnection();
    NetworkSocket *l_socket = new NetworkSocket(socket);

    // A connection that died while queued emitted its signals before anyone
    // listened; without this check it would linger as a phantom client
    // forever, holding a player slot with no wire left to tear it down.
    if (!l_socket->isOpen()) {
        l_socket->deleteLater();
        return;
    }

    // Too many players. Reject connection!
    // This also enforces the maximum playercount.
    if (m_available_ids.empty()) {
        akashi::Packet disconnect_reason("BD", {"Maximum playercount has been reached."});
        l_socket->write(disconnect_reason);
        connect(l_socket, &akashi::ITransport::clientDisconnected, l_socket, &QObject::deleteLater);
        l_socket->close();
        return;
    }

    int user_id = m_available_ids.pop();
    AOClient *client = new AOClient(this, l_socket, nullptr, user_id, music_manager);
    m_clients_ids.insert(user_id, client);

    int multiclient_count = 1;
    bool is_at_multiclient_limit = false;
    client->calculateIpid();
    auto ban = db_manager->isIPBanned(client->ipid());
    bool is_banned = ban.first;
    for (AOClient *joined_client : qAsConst(m_clients)) {
        if (client->remoteIp().isEqual(joined_client->remoteIp()))
            multiclient_count++;
    }

    if (multiclient_count > ConfigManager::multiClientLimit() && !client->remoteIp().isLoopback())
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
        markIDFree(user_id);
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
        markIDFree(user_id);
        connect(l_socket, &akashi::ITransport::clientDisconnected, client, &QObject::deleteLater);
        l_socket->close();
        return;
    }

    m_clients.append(client);
    connect(client, &AOClient::disconnected, this, [this, client] {
        if (client->isJoined()) {
            decreasePlayerCount();
        }
        m_clients.removeAll(client);
        // Withdraw the client's presence while it is alive, then delete it -
        // the destructor never does the real teardown.
        client->leave();
        client->deleteLater();
    });

    // This is the infamous workaround for
    // tsuserver4. It should disable fantacrypt
    // completely in any client 2.4.3 or newer
    akashi::Packet decryptor("decryptor", {"NOENCRYPT"});
    client->sendPacket(decryptor);
    hookupAOClient(client);
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

    for (AOClient *l_client : qAsConst(m_clients)) {
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

void Server::reloadSettings()
{
    ConfigManager::reloadSettings();
    Q_EMIT reloadRequest(ConfigManager::serverNickname(), ConfigManager::serverDescription());
    Q_EMIT updateHTTPConfiguration();
    handleDiscordIntegration();
    logger->loadLogtext();
    music_manager->reloadRequest();
    m_ipban_list = ConfigManager::iprangeBans();
    m_banned_asns = ConfigManager::bannedAsns();
    if (QFile::exists("storage/GeoLite2-ASN.mmdb")) {
        m_asn_reader.open("storage/GeoLite2-ASN.mmdb");
    }
    acl_roles_handler->loadFile(ConfigManager::path("acl_roles.json"));
    command_extension_collection->loadFile(ConfigManager::path("command_extensions.json"));
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
    for (AOClient *l_client : qAsConst(m_clients)) {
        l_client->sendPacket(packet);
    }
}

void Server::broadcast(const akashi::Packet &packet, TARGET_TYPE target)
{
    for (AOClient *l_client : qAsConst(m_clients)) {
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
        for (AOClient *l_client : qAsConst(m_clients)) {
            if (l_client->isGlobalEnabled()) {
                if (l_client->isAuthenticated()) {
                    l_client->sendPacket(other_packet);
                }
                else {
                    l_client->sendPacket(packet);
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
    for (AOClient *l_client : qAsConst(m_clients)) {
        if (l_client->ipid() == ipid)
            return_clients.append(l_client);
    }
    return return_clients;
}

QList<AOClient *> Server::clientsByHwid(QString f_hwid)
{
    QList<AOClient *> return_clients;
    for (AOClient *l_client : qAsConst(m_clients)) {
        if (l_client->hwid() == f_hwid)
            return_clients.append(l_client);
    }
    return return_clients;
}

AOClient *Server::clientById(int id)
{
    return m_clients_ids.value(id);
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

QQueue<QString> Server::areaBuffer(const QString &f_areaName)
{
    return logger->buffer(f_areaName);
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

QStringList Server::musicList()
{
    return m_music_list;
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

MedievalParser *Server::medievalParser()
{
    return medieval_parser;
}

ACLRolesHandler *Server::aclRolesHandler()
{
    return acl_roles_handler;
}

CommandExtensionCollection *Server::commandExtensionCollection()
{
    return command_extension_collection;
}

void Server::allowMessage()
{
    m_can_send_ic_messages = true;
}

void Server::handleDiscordIntegration()
{
    // Prevent double connecting by preemtively disconnecting them.
    disconnect(this, nullptr, discord, nullptr);

    if (ConfigManager::discordWebhookEnabled()) {
        if (ConfigManager::discordModcallWebhookEnabled())
            connect(this, &Server::modcallWebhookRequest, discord, &Discord::onModcallWebhookRequested);

        if (ConfigManager::discordBanWebhookEnabled())
            connect(this, &Server::banWebhookRequest, discord, &Discord::onBanWebhookRequested);
    }
    return;
}

void Server::markIDFree(const int &f_user_id)
{
    AOClient *l_client = m_clients_ids[f_user_id];
    if (l_client && l_client->isJoined()) {
        m_player_state_observer.unregisterClient(l_client);
    }
    m_clients_ids.insert(f_user_id, nullptr);
    m_available_ids.push(f_user_id);
}

void Server::hookupAOClient(AOClient *client)
{
    connect(client, &AOClient::joined, this, &Server::increasePlayerCount);
    connect(client, &AOClient::logIC, logger, &ULogger::logIC);
    connect(client, &AOClient::logMusic, logger, &ULogger::logMusic);
    connect(client, &AOClient::logOOC, logger, &ULogger::logOOC);
    connect(client, &AOClient::logLogin, logger, &ULogger::logLogin);
    connect(client, &AOClient::logCMD, logger, &ULogger::logCMD);
    connect(client, &AOClient::logBan, logger, &ULogger::logBan);
    connect(client, &AOClient::logKick, logger, &ULogger::logKick);
    connect(client, &AOClient::logModcall, logger, &ULogger::logModcall);
    connect(client, &AOClient::clientSuccessfullyDisconnected, this, &Server::markIDFree);
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

Server::~Server()
{
    // Empty the roster first so no teardown broadcast writes to a neighbour
    // mid-destruction, then delete synchronously - a deleteLater posted here
    // never runs, because the event loop is already gone at this point.
    const QVector<AOClient *> l_clients = m_clients;
    m_clients.clear();
    qDeleteAll(l_clients);

    delete server;
    delete discord;
    delete acl_roles_handler;
    delete db_manager;
    delete m_filesystem;
}
