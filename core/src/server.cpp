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

Server::Server(int p_port, int p_ws_port, QObject* parent) :
    QObject(parent),
    port(p_port),
    ws_port(p_ws_port)
{
    server = new QTcpServer(this);
    connect(server, SIGNAL(newConnection()), this, SLOT(clientConnected()));

    proxy = new WSProxy(port, ws_port, this);
    if(ws_port != -1)
        proxy->start();

    server_data = new ServerData;

    server_data->timer = new QTimer();

    server_data->db_manager = new DBManager();

    //We create it, even if its not used later on.
    discord = new Discord(this);

    logger = new ULogger(this);
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
    
    //Checks if any Discord webhooks are enabled.
    handleDiscordIntegration();

    //Construct modern advertiser if enabled in config
    if (ConfigManager::advertiseHTTPServer()) {
        httpAdvertiserTimer = new QTimer(this);
        httpAdvertiser = new HTTPAdvertiser();

        connect(httpAdvertiserTimer, &QTimer::timeout,
                httpAdvertiser, &HTTPAdvertiser::msAdvertiseServer);
        connect(this, &Server::setHTTPConfiguration,
                httpAdvertiser, &HTTPAdvertiser::setAdvertiserSettings);
        connect(this, &Server::updateHTTPConfiguration,
                httpAdvertiser, &HTTPAdvertiser::updateAdvertiserSettings);
        setHTTPAdvertiserConfig();
        httpAdvertiserTimer->start(300000);
    }

    //Get characters from config file
    server_data->m_characters = ConfigManager::charlist();

    //Get musiclist from config file
    server_data->m_music_list = ConfigManager::musiclist();

    //Get backgrounds from config file
    server_data->m_backgrounds = ConfigManager::backgrounds();

    //Assembles the area list
    server_data->m_area_names = ConfigManager::sanitizedAreaNames();
    QStringList raw_area_names = ConfigManager::rawAreaNames();
    for (int i = 0; i < raw_area_names.length(); i++) {
        QString area_name = raw_area_names[i];
        AreaData* l_area = new AreaData(area_name, i);
        server_data->m_areas.insert(i, l_area);
        connect(l_area, &AreaData::playJukeboxSong,
                this, QOverload<AOPacket,int>::of(&Server::broadcast));
    }

    //Loads the command help information. This is not stored inside the server.
    ConfigManager::loadCommandHelp();

    //Rate-Limiter for IC-Chat
    connect(&server_data->next_message_timer, SIGNAL(timeout()), this, SLOT(allowMessage()));
}

void Server::clientConnected()
{
    QTcpSocket* socket = server->nextPendingConnection();
    int user_id;
    QList<int> user_ids;
    for (AOClient* client : qAsConst(server_data->m_clients)) {
        user_ids.append(client->m_id);
    }
    for (user_id = 0; user_id <= server_data->m_player_count; user_id++) {
        if (user_ids.contains(user_id))
            continue;
        else
            break;
    }
    AOClient* client = new AOClient(server_data, socket, this, user_id);

    int multiclient_count = 1;
    bool is_at_multiclient_limit = false;
    client->calculateIpid();
    auto ban = server_data->db_manager->isIPBanned(client->getIpid());
    bool is_banned = ban.first;
    for (AOClient* joined_client : qAsConst(server_data->m_clients)) {
        if (client->m_remote_ip.isEqual(joined_client->m_remote_ip))
            multiclient_count++;
    }

    if (multiclient_count > ConfigManager::multiClientLimit() && !client->m_remote_ip.isLoopback()) // TODO: make this configurable
        is_at_multiclient_limit = true;

    if (is_banned) {
        QString reason = ban.second;
        AOPacket ban_reason("BD", {reason});
        socket->write(ban_reason.toUtf8());
    }
    if (is_banned || is_at_multiclient_limit) {
        socket->flush();
        client->deleteLater();
        socket->close();
        return;
    }

    server_data->m_clients.append(client);
    server_data->m_clients.append(client);
    connect(socket, &QTcpSocket::disconnected, client,
            &AOClient::clientDisconnected);
    connect(socket, &QTcpSocket::disconnected, this, [=] {
        server_data->m_clients.removeAll(client);
        client->deleteLater();
    });
    connect(socket, &QTcpSocket::readyRead, client, &AOClient::clientData);
    AOPacket decryptor("decryptor", {"NOENCRYPT"}); // This is the infamous workaround for
                                                    // tsuserver4. It should disable fantacrypt
                                                    // completely in any client 2.4.3 or newer
    client->sendPacket(decryptor);
    hookupAOClient(client);
#ifdef NET_DEBUG
    qDebug() << client->remote_ip.toString() << "connected";
#endif
}

void Server::broadcast(AOPacket packet, int area_index)
{
    for (AOClient* client : qAsConst(server_data->m_clients)) {
        if (client->m_current_area == area_index)
            client->sendPacket(packet);
    }
}

void Server::broadcast(AOPacket packet)
{
    for (AOClient* client : qAsConst(server_data->m_clients)) {
        client->sendPacket(packet);
    }
}

void Server::onModcallWebhookRequest(const QString &f_name, const QString &f_area, const QString &f_reason)
{
    emit modcallWebhookRequest(f_name, f_area, f_reason, getAreaBuffer(f_area));
}

void Server::onBanWebhookRequest(const QString& f_ipid, const QString& f_moderator, const QString& f_ban_duration, const QString& f_reason, const int& f_ban_id)
{
    emit banWebhookRequest(f_ipid, f_moderator, f_ban_duration, f_reason, f_ban_id);
}

void Server::setHTTPAdvertiserConfig()
{
    advertiser_config config;
    config.name = ConfigManager::serverName();
    config.description = ConfigManager::serverDescription();
    config.port = ConfigManager::serverPort();
    config.ws_port = ConfigManager::webaoPort();
    config.players = ConfigManager::maxPlayers();
    config.masterserver = ConfigManager::advertiserHTTPIP();
    config.debug = ConfigManager::advertiserHTTPDebug();
    emit setHTTPConfiguration(config);
}

void Server::updateHTTPAdvertiserConfig()
{
    update_advertiser_config config;
    config.name = ConfigManager::serverName();
    config.description = ConfigManager::serverDescription();
    config.players = ConfigManager::maxPlayers();
    config.masterserver = ConfigManager::advertiserHTTPIP();
    config.debug = ConfigManager::advertiserHTTPDebug();
    emit updateHTTPConfiguration(config);

}

QQueue<QString> Server::getAreaBuffer(const QString &f_areaName)
{
    return logger->buffer(f_areaName);
}

void Server::allowMessage()
{
    server_data->can_send_ic_messages = true;
}

void Server::onReloadRequest()
{
    ConfigManager::reloadSettings();
    emit reloadRequest(ConfigManager::serverName(), ConfigManager::serverDescription());
    server_data->m_music_list = ConfigManager::musiclist();
    updateHTTPAdvertiserConfig();
    handleDiscordIntegration();
}

void Server::handleDiscordIntegration()
{
     // Prevent double connecting by preemtively disconnecting them.
    disconnect(this, nullptr, discord, nullptr);

    if (ConfigManager::discordWebhookEnabled()) {
        if (ConfigManager::discordModcallWebhookEnabled())
            connect(this, &Server::modcallWebhookRequest,
                    discord, &Discord::onModcallWebhookRequested);

        if (ConfigManager::discordBanWebhookEnabled())
            connect(this, &Server::banWebhookRequest,
                    discord, &Discord::onBanWebhookRequested);

        if (ConfigManager::discordUptimeEnabled())
            discord->startUptimeTimer();
        else
            discord->stopUptimeTimer();
    }
    return;
}

void Server::hookupAOClient(AOClient* client)
{
    //Broadcast functions are overloaded.
    connect(client, &AOClient::broadcastToArea, this, QOverload<AOPacket,int>::of(&Server::broadcast));
    connect(client, &AOClient::broadcastToServer, this, QOverload<AOPacket>::of(&Server::broadcast));
    connect(client, &AOClient::reloadServer, this, &Server::onReloadRequest);

    //Relevant Discord Webhook connections.
    connect(client, &AOClient::modcallWebhookRequest, this, &Server::onModcallWebhookRequest);
    connect(client, &AOClient::banWebhookRequest, this, &Server::onBanWebhookRequest);

    //This needs to be connected per client.
    connect(client, &AOClient::logIC, logger, &ULogger::logIC);
    connect(client, &AOClient::logOOC, logger, &ULogger::logOOC);
    connect(client, &AOClient::logLogin, logger, &ULogger::logLogin);
    connect(client, &AOClient::logCMD, logger, &ULogger::logCMD);
    connect(client, &AOClient::logBan, logger, &ULogger::logBan);
    connect(client, &AOClient::logKick, logger, &ULogger::logKick);
    connect(client, &AOClient::logModcall, logger, &ULogger::logModcall);
    connect(client, &AOClient::logConnectionAttempt, logger, &ULogger::logConnectionAttempt);
}

Server::~Server()
{
    for (AOClient* client : qAsConst(server_data->m_clients)) {
        client->deleteLater();
    }
    server->deleteLater();
    proxy->deleteLater();
    discord->deleteLater();
}
