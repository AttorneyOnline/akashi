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
#ifndef SERVER_H
#define SERVER_H

#include "include/aoclient.h"
#include "include/aopacket.h"
#include "include/area_data.h"
#include "include/ws_proxy.h"
#include "include/db_manager.h"
#include "include/discord.h"
#include "include/config_manager.h"
#include "include/http_advertiser.h"
#include "include/logger/u_logger.h"
#include "include/server_data.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QMap>
#include <QSettings>
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

class AOClient;
class DBManager;
class AreaData;

/**
 * @brief The class that represents the actual server as it is.
 */
class Server : public QObject {
    Q_OBJECT

  public:
    /**
     * @brief Creates a Server instance.
     *
     * @param p_port The TCP port to listen for connections on.
     * @param p_ws_port The WebSocket port to listen for connections on.
     * @param parent Qt-based parent, passed along to inherited constructor from QObject.
     */
    Server(int p_port, int p_ws_port, QObject* parent = nullptr);

    /**
      * @brief Destructor for the Server class.
      *
      * @details Marks every Client, the WSProxy, the underlying #server, and the database manager to be deleted later.
      */
    ~Server();

    /**
     * @brief Starts the server.
     *
     * @details Amongst other things, this function starts the listening on the given TCP port, sets up the server
     * according to the configuration file, and starts listening on the WebSocket port if it is not `-1`.
     *
     * Advertising is not done here -- see Advertiser::contactMasterServer() for that.
     */
    void start();

    /**
     * @brief Returns the character's character ID (= their index in the character list).
     *
     * @param char_name The 'internal' name for the character whose character ID to look up. This is equivalent to
     * the name of the directory of the character.
     *
     * @return The character ID if a character with that name exists in the character selection list, `-1` if not.
     */
    int getCharID(QString char_name);

    /**
     * @brief Creates an HTTP advertiser config struct and emits it using server::reloadHTTPRequest.
     */
    void setHTTPAdvertiserConfig();

    /**
     * @brief Updates the modern advertiser configuration on configuration reload.
     */
    void updateHTTPAdvertiserConfig();

    /**
     * @brief Getter for an area specific buffer from the logger.
     */
    QQueue<QString> getAreaBuffer(const QString& f_areaName);

  public slots:
    /**
     * @brief Handles a new connection.
     *
     * @details The function creates an AOClient to represent the user, assigns a user ID to them, and
     * checks if the client is banned.
     */
    void clientConnected();

    /**
     * @brief Sets #can_send_messages to true.
     *
     * @details Called whenever #next_message_timer reaches 0.
     */
    void allowMessage();

    /**
     * @brief Reloads the server information on SUPER user request.
     */
    void onReloadRequest();

    /**
     * @brief Sends a packet to all clients in a given area.
     *
     * @param packet The packet to send to the clients.
     * @param area_index The index of the area to look for clients in.
     *
     * @note Does nothing if an area by the given index does not exist.
     */
    void broadcast(AOPacket packet, int area_index);

    /**
     * @brief Sends a packet to all clients in the server.
     *
     * @param packet The packet to send to the clients.
     */
    void broadcast(AOPacket packet);

    /**
     * @brief onModcallWebhookRequest
     * @param Name of the client requesting a mod presence.
     * @param Area of the modcall happening.
     * @param Reason for the modcall.
     */
    void onModcallWebhookRequest(const QString& f_name, const QString& f_area, const QString& f_reason);

    /**
    * @brief Signals a ban webhook request to the server.
    * @param f_ipid IPID of the client being banned.
    * @param f_moderator Moderator issuing the ban.
    * @param f_ban_duration The duration of the ban.
    * @param f_reason The reason of the ban.
    * @param f_ban_id The ban ID.
    */
    void onBanWebhookRequest(const QString& f_ipid, const QString& f_moderator, const QString& f_ban_duration, const QString& f_reason, const int& f_ban_id);

  signals:

    /**
     * @brief Sends the server name and description, emitted by /reload.
     *
     * @param p_name The server name.
     * @param p_desc The server description.
     */
    void reloadRequest(QString p_name, QString p_desc);

    /**
     * @brief Sends all necessary info for the modern advertiser.
     * @param Struct that contains all configuration for the advertiser
     */
    void setHTTPConfiguration(struct advertiser_config config);

    /**
     * @brief Sends a partial update to the modern advertiser.
     * @param Struct that contains partial information about the server to update the advertised information.
     */
    void updateHTTPConfiguration(struct update_advertiser_config config);

    /**
     * @brief Sends a modcall webhook request, emitted by AOClient::pktModcall.
     *
     * @param f_name The character or OOC name of the client who sent the modcall.
     * @param f_area The name of the area the modcall was sent from.
     * @param f_reason The reason the client specified for the modcall.
     * @param f_buffer The area's log buffer.
     */
    void modcallWebhookRequest(const QString& f_name, const QString& f_area, const QString& f_reason, const QQueue<QString>& f_buffer);

    /**
     * @brief Sends a ban webhook request, emitted by AOClient::cmdBan
     * @param f_ipid The IPID of the banned client.
     * @param f_moderator The moderator who issued the ban.
     * @param f_duration The duration of the ban in a human readable format.
     * @param f_reason The reason for the ban.
     * @param f_banID The ID of the issued ban.
     */
    void banWebhookRequest(const QString& f_ipid, const QString& f_moderator, const QString& f_duration, const QString& f_reason, const int& f_banID);

  private:
    /**
     * @brief Connects new AOClient to required signals.
     **/
    void hookupAOClient(AOClient* client);

    /**
     * @brief Method to construct and reconstruct Discord Webhook Integration.
     *
     * @details Constructs or rebuilds Discord Object during server startup and configuration reload.
     */
    void handleDiscordIntegration();

    /**
     * @brief The proxy used for WebSocket connections.
     *
     * @see WSProxy and WSClient for an explanation as to why this is a thing.
     */
    WSProxy* proxy;

    /**
     * @brief Listens for incoming TCP connections.
     */
    QTcpServer* server;

    /**
     * @brief Handles Discord webhooks.
     */
    Discord* discord;

    /**
     * @brief Handles HTTP server advertising.
     */
    HTTPAdvertiser* httpAdvertiser;

    /**
     * @brief Advertises the server in a regular intervall.
     */
    QTimer* httpAdvertiserTimer;

    /**
     * @brief Handles the universal log framework.
     */
    ULogger* logger;

    /**
     * @brief The port through which the server will accept TCP connections.
     */
    int port;

    /**
     * @brief The port through which the server will accept WebSocket connections.
     */
    int ws_port;

    ServerData* server_data;
};

#endif // SERVER_H
