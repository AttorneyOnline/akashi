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
     * @brief Gets a pointer to a client by IPID.
     *
     * @param ipid The IPID to look for.
     *
     * @return A pointer to the client if found, a nullpointer if not.
     *
     * @see Server::getClientsByIpid() to get all clients ran by the same user.
     */
    AOClient* getClient(QString ipid);

    /**
     * @brief Gets a list of pointers to all clients with the given IPID.
     *
     * @param ipid The IPID to look for.
     *
     * @return A list of clients whose IPID match. List may be empty.
     */
    QList<AOClient*> getClientsByIpid(QString ipid);
    
    /**
     * @brief Gets a pointer to a client by user ID.
     *
     * @param id The user ID to look for.
     *
     * @return A pointer to the client if found, a nullpointer if not.
     */
    AOClient* getClientByID(int id);

    /**
     * @brief Updates which characters are taken in the given area, and sends out an update packet to
     * all clients present the area.
     *
     * @param area The area in which to update the list of characters.
     */
    void updateCharsTaken(AreaData* area);

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
     * @brief Returns the character's character ID (= their index in the character list).
     *
     * @param char_name The 'internal' name for the character whose character ID to look up. This is equivalent to
     * the name of the directory of the character.
     *
     * @return The character ID if a character with that name exists in the character selection list, `-1` if not.
     */
    int getCharID(QString char_name);

    /**
     * @brief The collection of all currently connected clients.
     */
    QVector<AOClient*> clients;

    /**
     * @brief The overall player count in the server.
     */
    int player_count;

    /**
     * @brief The characters available on the server to use.
     */
    QStringList characters;

    /**
     * @brief The areas on the server.
     */
    QVector<AreaData*> areas;

    /**
     * @brief The names of the areas on the server.
     *
     * @details Equivalent to iterating over #areas and getting the area names individually, but grouped together
     * here for faster access.
     */
    QStringList area_names;

    /**
     * @brief The available songs on the server.
     *
     * @details Does **not** include the area names, the actual music list packet should be constructed from
     * #area_names and this combined.
     */
    QStringList music_list;
  
    /**
     * @brief The base URL to use when generating streaming URLs to send. e.g. https://www.example.com/stream/
     */
    QUrl base_streaming_url;

    /**
     * @brief The backgrounds on the server that may be used in areas.
     */
    QStringList backgrounds;

    /**
     * @brief The database manager on the server, used to store users' bans and authorisation details.
     */
    DBManager* db_manager;

    /**
     * @brief The max amount of players on the server.
     */
    QString max_players;
    /**
     * @brief The user-facing server name.
     */
    QString server_name;

    /**
     * @brief The server description.
     */
    QString server_desc;

    /**
     * @brief The Message Of The Day of the server, shown upon entry to the server and on request.
     */
    QString MOTD;

   /**
    * @brief The Maximum amounts of IC-Messages an area is allowed to store.
    */
    int maximum_statements;

    /**
     * @brief The authorization type of the server.
     *
     * @details In simple mode, the modpass stored in config.ini is used for moderator logins. In advanced mode, logins found in the database are used.
     */
    QString auth_type;

    /**
     * @brief The modpass for moderator login with simple auth_type.
     */
    QString modpass;

    /**
     * @brief The amount of subscripts zalgo is stripped by.
     */
    int zalgo_tolerance;

    /**
     * @brief The highest value dice can have.
     */
    uint dice_value;

    /**
     * @brief The max amount of dice that can be rolled at once.
     */
    int max_dice;

    /**
     * @brief The server-wide global timer.
     */
    QTimer* timer;

    /**
     * @brief Loads values from config.ini.
     */
    void loadServerConfig();

    /**
     * @brief Loads the configuration files for commands into stringlists.
     */
    void loadCommandConfig();

    /**
     * @brief Returns a stringlist with the contents of a .txt file from config/text/.
     *
     * @param Name of the file to load.
     */
    QStringList loadConfigFile(QString filename);

    /**
     * @brief List holding the contents of 8ball.txt, used by /8ball.
     */
    QStringList magic_8ball_answers;

    /**
     * @brief List holding the contents of praise.txt, used by AOClient::getReprimand.
     */
    QStringList praise_list;

    /**
     * @brief List holding the contents of reprimands.txt, used by AOClient::getReprimand.
     */
    QStringList reprimands_list;

    /**
     * @brief List holding the contents of gimp.txt, used by AOClient::cmdGimp.
     */
    QStringList gimp_list;

  public slots:
    /**
     * @brief Handles a new connection.
     *
     * @details The function creates an AOClient to represent the user, assigns a user ID to them, and
     * checks if the client is banned.
     */
    void clientConnected();

  signals:

    /**
     * @brief Sends the server name and description, emitted by /reload.
     *
     * @param p_name The server name.
     * @param p_desc The server description.
     */
    void reloadRequest(QString p_name, QString p_desc);

  private:
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
     * @brief The port through which the server will accept TCP connections.
     */
    int port;

    /**
     * @brief The port through which the server will accept WebSocket connections.
     */
    int ws_port;
};

#endif // SERVER_H
