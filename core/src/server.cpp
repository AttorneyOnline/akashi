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
#include "include/server.h"

#include "include/acl_roles_handler.h"
#include "include/advertiser.h"
#include "include/aoclient.h"
#include "include/area_data.h"
#include "include/command_extension.h"
#include "include/config_manager.h"
#include "include/db_manager.h"
#include "include/discord.h"
#include "include/logger/u_logger.h"
#include "include/music_manager.h"
#include "include/network/network_socket.h"
#include "include/packet/packet_factory.h"

Server::Server(int p_port, int p_ws_port, QObject *parent) :
    QObject(parent),
    port(p_port),
    ws_port(p_ws_port),
    m_player_count(0)
{
    server = new QTcpServer(this);

    connect(server, &QTcpServer::newConnection, this, &Server::clientConnected);

    timer = new QTimer(this);

    db_manager = new DBManager;

    acl_roles_handler = new ACLRolesHandler(this);
    acl_roles_handler->loadFile("config/acl_roles.ini");

    command_extension_collection = new CommandExtensionCollection;
    command_extension_collection->setCommandNameWhitelist(AOClient::COMMANDS.keys());
    command_extension_collection->loadFile("config/command_extensions.ini");

    // We create it, even if its not used later on.
    discord = new Discord(this);

    logger = new ULogger(this);
    connect(this, &Server::logConnectionAttempt, logger, &ULogger::logConnectionAttempt);
}

void Server::start()
{
    QString bind_ip = ConfigManager::bindIP();
    QHostAddress bind_addr;
    if (bind_ip == "all")
        bind_addr = QHostAddress::Any;
    else
        bind_addr = QHostAddress(bind_ip);
    if (bind_addr.protocol() != QAbstractSocket::IPv4Protocol && bind_addr.protocol() != QAbstractSocket::IPv6Protocol && bind_addr != QHostAddress::Any) {
        qDebug() << bind_ip << "is an invalid IP address to listen on! Server not starting, check your config.";
    }
    if (!server->listen(bind_addr, port)) {
        qDebug() << "Server error:" << server->errorString();
    }
    else {
        qDebug() << "Server listening on" << port;
    }

    // Enable WebAO
    if (ConfigManager::webaoEnabled()) {
        ws_server = new QWebSocketServer("Akashi", QWebSocketServer::NonSecureMode, this);
        if (!ws_server->listen(bind_addr, ConfigManager::webaoPort())) {
            qDebug() << "Websocket Server error:" << ws_server->errorString();
        }
        else {
            connect(ws_server, &QWebSocketServer::newConnection,
                    this, &Server::ws_clientConnected);
            qInfo() << "Websocket Server listening on" << ConfigManager::webaoPort();
        }
    }

    // Checks if any Discord webhooks are enabled.
    handleDiscordIntegration();

    // Construct modern advertiser if enabled in config
    if (ConfigManager::advertiseServer()) {
        AdvertiserTimer = new QTimer(this);
        ms3_Advertiser = new Advertiser();

        connect(AdvertiserTimer, &QTimer::timeout, ms3_Advertiser, &Advertiser::msAdvertiseServer);
        connect(this, &Server::playerCountUpdated, ms3_Advertiser, &Advertiser::updatePlayerCount);
        connect(this, &Server::updateHTTPConfiguration, ms3_Advertiser, &Advertiser::updateAdvertiserSettings);
        emit playerCountUpdated(m_player_count);
        ms3_Advertiser->msAdvertiseServer();
        AdvertiserTimer->start(300000);
    }

    // Get characters from config file
    m_characters = ConfigManager::charlist();

    // Get backgrounds from config file
    m_backgrounds = ConfigManager::backgrounds();

    // Build our music manager.
    ConfigManager::musiclist();
    music_manager = new MusicManager(ConfigManager::ordered_songs(), ConfigManager::cdnList(), ConfigManager::musiclist(), this);
    connect(music_manager, &MusicManager::sendFMPacket, this, &Server::unicast);
    connect(music_manager, &MusicManager::sendAreaFMPacket, this, QOverload<AOPacket*, int>::of(&Server::broadcast));

    // Get musiclist from config file
    m_music_list = music_manager->rootMusiclist();

    // Assembles the area list
    m_area_names = ConfigManager::sanitizedAreaNames();
    QStringList raw_area_names = ConfigManager::rawAreaNames();
    for (int i = 0; i < raw_area_names.length(); i++) {
        QString area_name = raw_area_names[i];
        AreaData *l_area = new AreaData(area_name, i, music_manager);
        m_areas.insert(i, l_area);
        connect(l_area, &AreaData::sendAreaPacket, this, QOverload<AOPacket*, int>::of(&Server::broadcast));
        connect(l_area, &AreaData::sendAreaPacketClient, this, &Server::unicast);
        connect(l_area, &AreaData::userJoinedArea, music_manager, &MusicManager::userJoinedArea);
        music_manager->registerArea(i);
    }

    // Loads the command help information. This is not stored inside the server.
    ConfigManager::loadCommandHelp();

    // Get IP bans
    m_ipban_list = ConfigManager::iprangeBans();

    // Rate-Limiter for IC-Chat
    m_message_floodguard_timer = new QTimer(this);
    connect(m_message_floodguard_timer, &QTimer::timeout, this, &Server::allowMessage);

    // Prepare player IDs and reference hash.
    for (int i = ConfigManager::maxPlayers() - 1; i >= 0; i--) {
        m_available_ids.push(i);
        m_clients_ids.insert(i, nullptr);
    }
}

QVector<AOClient *> Server::getClients()
{
    return m_clients;
}

void Server::clientConnected()
{
    QTcpSocket *socket = server->nextPendingConnection();

    // Too many players. Reject connection!
    // This also enforces the maximum playercount.
    if (m_available_ids.empty()) {
        AOPacket *disconnect_reason = PacketFactory::createPacket("BD", {"Maximum playercount has been reached."});
        socket->write(disconnect_reason->toUtf8());
        socket->flush();
        socket->close();
        socket->deleteLater();
        return;
    }

    int user_id = m_available_ids.pop();
    // The parent hierachry looks like this :
    // QTcpSocket -> NetworkSocket -> AOClient
    NetworkSocket *l_socket = new NetworkSocket(socket, socket);
    AOClient *client = new AOClient(this, l_socket, l_socket, user_id, music_manager);
    m_clients_ids.insert(user_id, client);

    int multiclient_count = 1;
    bool is_at_multiclient_limit = false;
    client->calculateIpid();
    auto ban = db_manager->isIPBanned(client->getIpid());
    bool is_banned = ban.first;
    for (AOClient *joined_client : qAsConst(m_clients)) {
        if (client->m_remote_ip.isEqual(joined_client->m_remote_ip))
            multiclient_count++;
    }

    if (multiclient_count > ConfigManager::multiClientLimit() && !client->m_remote_ip.isLoopback())
        is_at_multiclient_limit = true;

    if (is_banned) {
        QString reason = ban.second;
        AOPacket *ban_reason = PacketFactory::createPacket("BD", {reason});
        socket->write(ban_reason->toUtf8());
    }
    if (is_banned || is_at_multiclient_limit) {
        socket->flush();
        client->deleteLater();
        socket->close();
        markIDFree(user_id);
        return;
    }

    QHostAddress l_remote_ip = client->m_remote_ip;
    if (l_remote_ip.protocol() == QAbstractSocket::IPv6Protocol) {
        l_remote_ip = parseToIPv4(l_remote_ip);
    }

    if (isIPBanned(l_remote_ip)) {
        QString l_reason = "Your IP has been banned by a moderator.";
        AOPacket *l_ban_reason = PacketFactory::createPacket("BD", {l_reason});
        socket->write(l_ban_reason->toUtf8());
        client->deleteLater();
        socket->close();
        markIDFree(user_id);
        return;
    }

    m_clients.append(client);
    connect(l_socket, &NetworkSocket::clientDisconnected, this, [=] {
        if (client->hasJoined()) {
            decreasePlayerCount();
        }
        m_clients.removeAll(client);
        client->deleteLater();
    });
    connect(l_socket, &NetworkSocket::handlePacket, client, &AOClient::handlePacket);

    // This is the infamous workaround for
    // tsuserver4. It should disable fantacrypt
    // completely in any client 2.4.3 or newer
    AOPacket *decryptor = PacketFactory::createPacket("decryptor", {"NOENCRYPT"});
    client->sendPacket(decryptor);
    hookupAOClient(client);
#ifdef NET_DEBUG
    qDebug() << client->m_remote_ip.toString() << "connected";
#endif
}

void Server::ws_clientConnected()
{
    QWebSocket *socket = ws_server->nextPendingConnection();
    NetworkSocket *l_socket = new NetworkSocket(socket, socket);

    // Too many players. Reject connection!
    // This also enforces the maximum playercount.
    if (m_available_ids.empty()) {
        AOPacket *disconnect_reason = PacketFactory::createPacket("BD", {"Maximum playercount has been reached."});
        l_socket->write(disconnect_reason);
        l_socket->close();
        l_socket->deleteLater();
        return;
    }

    int user_id = m_available_ids.pop();
    AOClient *client = new AOClient(this, l_socket, l_socket, user_id, music_manager);
    m_clients_ids.insert(user_id, client);

    int multiclient_count = 1;
    bool is_at_multiclient_limit = false;
    client->calculateIpid();
    auto ban = db_manager->isIPBanned(client->getIpid());
    bool is_banned = ban.first;
    for (AOClient *joined_client : qAsConst(m_clients)) {
        if (client->m_remote_ip.isEqual(joined_client->m_remote_ip))
            multiclient_count++;
    }

    if (multiclient_count > ConfigManager::multiClientLimit() && !client->m_remote_ip.isLoopback())
        is_at_multiclient_limit = true;

    if (is_banned) {
        QString reason = ban.second;
        AOPacket *ban_reason = PacketFactory::createPacket("BD", {reason});
        socket->sendTextMessage(ban_reason->toUtf8());
    }
    if (is_banned || is_at_multiclient_limit) {
        client->deleteLater();
        l_socket->close(QWebSocketProtocol::CloseCodeNormal);
        markIDFree(user_id);
        return;
    }

    QHostAddress l_remote_ip = client->m_remote_ip;
    if (l_remote_ip.protocol() == QAbstractSocket::IPv6Protocol) {
        l_remote_ip = parseToIPv4(l_remote_ip);
    }

    if (isIPBanned(l_remote_ip)) {
        QString l_reason = "Your IP has been banned by a moderator.";
        AOPacket *l_ban_reason = PacketFactory::createPacket("BD", {l_reason});
        l_socket->write(l_ban_reason);
        client->deleteLater();
        l_socket->close(QWebSocketProtocol::CloseCodeNormal);
        markIDFree(user_id);
        return;
    }

    m_clients.append(client);
    connect(l_socket, &NetworkSocket::clientDisconnected, this, [=] {
        if (client->hasJoined()) {
            decreasePlayerCount();
        }
        m_clients.removeAll(client);
        l_socket->deleteLater();
    });
    connect(l_socket, &NetworkSocket::handlePacket, client, &AOClient::handlePacket);

    // This is the infamous workaround for
    // tsuserver4. It should disable fantacrypt
    // completely in any client 2.4.3 or newer
    AOPacket *decryptor = PacketFactory::createPacket("decryptor", {"NOENCRYPT"});
    client->sendPacket(decryptor);
    hookupAOClient(client);
}

void Server::updateCharsTaken(AreaData *area)
{
    QStringList chars_taken;
    for (const QString &cur_char : qAsConst(m_characters)) {
        chars_taken.append(area->charactersTaken().contains(getCharID(cur_char))
                               ? QStringLiteral("-1")
                               : QStringLiteral("0"));
    }

    AOPacket *response_cc = PacketFactory::createPacket("CharsCheck", chars_taken);

    for (AOClient *l_client : qAsConst(m_clients)) {
        if (l_client->m_current_area == area->index()) {
            if (!l_client->m_is_charcursed)
                l_client->sendPacket(response_cc);
            else {
                QStringList chars_taken_cursed = getCursedCharsTaken(l_client, chars_taken);
                AOPacket *response_cc_cursed = PacketFactory::createPacket("CharsCheck", chars_taken_cursed);
                l_client->sendPacket(response_cc_cursed);
            }
        }
    }
}

QStringList Server::getCursedCharsTaken(AOClient *client, QStringList chars_taken)
{
    QStringList chars_taken_cursed;
    for (int i = 0; i < chars_taken.length(); i++) {
        if (!client->m_charcurse_list.contains(i))
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

void Server::reloadSettings()
{
    ConfigManager::reloadSettings();
    emit reloadRequest(ConfigManager::serverName(), ConfigManager::serverDescription());
    emit updateHTTPConfiguration();
    handleDiscordIntegration();
    logger->loadLogtext();
    m_ipban_list = ConfigManager::iprangeBans();
    acl_roles_handler->loadFile("config/acl_roles.ini");
    command_extension_collection->loadFile("config/command_extensions.ini");
}

void Server::broadcast(AOPacket *packet, int area_index)
{
    QVector<int> l_client_ids = m_areas.value(area_index)->joinedIDs();
    for (const int l_client_id : qAsConst(l_client_ids)) {
        getClientByID(l_client_id)->sendPacket(packet);
    }
}

void Server::broadcast(AOPacket *packet)
{
    for (AOClient *l_client : qAsConst(m_clients)) {
        l_client->sendPacket(packet);
    }
}

void Server::broadcast(AOPacket *packet, TARGET_TYPE target)
{
    for (AOClient *l_client : qAsConst(m_clients)) {
        switch (target) {
        case TARGET_TYPE::MODCHAT:
            if (l_client->checkPermission(ACLRole::MODCHAT)) {
                l_client->sendPacket(packet);
            }
            break;
        case TARGET_TYPE::ADVERT:
            if (l_client->m_advert_enabled) {
                l_client->sendPacket(packet);
            }
            break;
        default:
            break;
        }
    }
}

void Server::broadcast(AOPacket *packet, AOPacket *other_packet, TARGET_TYPE target)
{
    switch (target) {
    case TARGET_TYPE::AUTHENTICATED:
        for (AOClient *l_client : qAsConst(m_clients)) {
            if (l_client->isAuthenticated()) {
                l_client->sendPacket(other_packet);
            }
            else {
                l_client->sendPacket(packet);
            }
        }
    default:
        // Unimplemented, so not handled.
        break;
    }
}

void Server::unicast(AOPacket *f_packet, int f_client_id)
{
    AOClient *l_client = getClientByID(f_client_id);
    if (l_client != nullptr) { // This should never happen, but safety first.
        l_client->sendPacket(f_packet);
        return;
    }
}

QList<AOClient *> Server::getClientsByIpid(QString ipid)
{
    QList<AOClient *> return_clients;
    for (AOClient *l_client : qAsConst(m_clients)) {
        if (l_client->getIpid() == ipid)
            return_clients.append(l_client);
    }
    return return_clients;
}

QList<AOClient *> Server::getClientsByHwid(QString f_hwid)
{
    QList<AOClient *> return_clients;
    for (AOClient *l_client : qAsConst(m_clients)) {
        if (l_client->getHwid() == f_hwid)
            return_clients.append(l_client);
    }
    return return_clients;
}

AOClient *Server::getClientByID(int id)
{
    return m_clients_ids.value(id);
}

int Server::getPlayerCount()
{
    return m_player_count;
}

QStringList Server::getCharacters()
{
    return m_characters;
}

int Server::getCharacterCount()
{
    return m_characters.length();
}

QString Server::getCharacterById(int f_chr_id)
{
    QString l_chr;

    if (f_chr_id >= 0 && f_chr_id < m_characters.length()) {
        l_chr = m_characters.at(f_chr_id);
    }

    return l_chr;
}

int Server::getCharID(QString char_name)
{
    for (const QString &character : qAsConst(m_characters)) {
        if (character.toLower() == char_name.toLower()) {
            return m_characters.indexOf(QRegExp(character, Qt::CaseInsensitive, QRegExp::FixedString));
        }
    }
    return -1; // character does not exist
}

QVector<AreaData *> Server::getAreas()
{
    return m_areas;
}

int Server::getAreaCount()
{
    return m_areas.length();
}

AreaData *Server::getAreaById(int f_area_id)
{
    AreaData *l_area = nullptr;

    if (f_area_id >= 0 && f_area_id < m_areas.length()) {
        l_area = m_areas.at(f_area_id);
    }

    return l_area;
}

QQueue<QString> Server::getAreaBuffer(const QString &f_areaName)
{
    return logger->buffer(f_areaName);
}

QStringList Server::getAreaNames()
{
    return m_area_names;
}

QString Server::getAreaName(int f_area_id)
{
    QString l_name;

    if (f_area_id >= 0 && f_area_id < m_area_names.length()) {
        l_name = m_area_names.at(f_area_id);
    }

    return l_name;
}

QStringList Server::getMusicList()
{
    return m_music_list;
}

QStringList Server::getBackgrounds()
{
    return m_backgrounds;
}

DBManager *Server::getDatabaseManager()
{
    return db_manager;
}

ACLRolesHandler *Server::getACLRolesHandler()
{
    return acl_roles_handler;
}

CommandExtensionCollection *Server::getCommandExtensionCollection()
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

        if (ConfigManager::discordUptimeEnabled())
            discord->startUptimeTimer();
        else
            discord->stopUptimeTimer();
    }
    return;
}

void Server::markIDFree(const int &f_user_id)
{
    m_available_ids.push(f_user_id);
    m_clients_ids.insert(f_user_id, nullptr);
}

void Server::hookupAOClient(AOClient *client)
{
    connect(client, &AOClient::joined, this, &Server::increasePlayerCount);
    connect(client, &AOClient::logIC, logger, &ULogger::logIC);
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
    emit playerCountUpdated(m_player_count);
}

void Server::decreasePlayerCount()
{
    m_player_count--;
    emit playerCountUpdated(m_player_count);
}

bool Server::isIPBanned(QHostAddress f_remote_IP)
{
    bool l_match_found = false;
    for (const QString &l_ipban : qAsConst(m_ipban_list)) {
        if (f_remote_IP.isInSubnet(QHostAddress::parseSubnet(l_ipban))) {
            l_match_found = true;
            break;
        }
    }
    return l_match_found;
}

Server::~Server()
{
    for (AOClient *l_client : qAsConst(m_clients)) {
        l_client->deleteLater();
    }
    server->deleteLater();
    discord->deleteLater();
    acl_roles_handler->deleteLater();

    delete db_manager;
}
