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
#pragma once

#include "akashi_core_export.h"
#include "core/exit_code.h"
#include "core/mmdb_reader.h"
#include "data_types.h"
#include "medieval_parser.h"
#include "player_directory.h"
#include "playerstateobserver.h"
#include "proto/packet.h"
#include "world/floor.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QMap>
#include <QRegularExpression>
#include <QSettings>
#include <QStack>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QWebSocket>
#include <QWebSocketServer>

#include <memory>

namespace akashi {
class ArupBroadcaster;
class AuthThrottle;
class CommandRegistry;
class ConfigStore;
class DatabaseService;
class EventBus;
class FileSystemService;
class LogService;
class PacketService;
class TextFilterRegistry;
class WriterText;
class PermissionRegistry;
class ServiceRegistry;
}

class ACLRolesHandler;
class DiscordSettings;
class ServerPublisher;
class ServerSettings;
class AOClient;
class AreaData;
class DBManager;
class Discord;

/**
 * @brief The class that represents the actual server as it is.
 */
class AKASHI_CORE_EXPORT Server : public QObject
{
    Q_OBJECT

  public:
    /**
     * @brief Creates a Server instance.
     *
     * @param p_ws_port The port to listen for connections on.
     * @param parent Qt-based parent, passed along to inherited constructor from QObject.
     */
    Server(int p_ws_port, akashi::ConfigStore *f_config_store, akashi::DatabaseService *f_database, akashi::ServiceRegistry *f_services, QObject *parent = nullptr);

    /**
     * @brief Destructor for the Server class.
     *
     * @details Marks every Client, the WSProxy, the underlying #server, and the database manager to be deleted later.
     */
    ~Server();

    /**
     * @brief Starts the server.
     *
     * @details Starts listening for incoming connections on the given port.
     *
     * @return ExitCode::Ok on success, otherwise the exit code describing the error.
     */
    ExitCode start();

    /**
     * @brief Enum to specifc different targets to send altered packets to a specific usergroup.
     */
    enum class TARGET_TYPE
    {
        AUTHENTICATED,
        MODCHAT,
        ADVERT
    };
    Q_ENUM(TARGET_TYPE)

    /**
     * @brief Returns a list of all clients currently in the server.
     *
     * @return A list of all clients currently in the server.
     */
    QVector<AOClient *> clients();

    /**
     * @brief Gets a pointer to a client by IPID.
     *
     * @param ipid The IPID to look for.
     *
     * @return A pointer to the client if found, a nullpointer if not.
     *
     * @see Server::clientsByIpid() to get all clients ran by the same user.
     */
    AOClient *client(QString ipid);

    /**
     * @brief Gets a list of pointers to all clients with the given IPID.
     *
     * @param ipid The IPID to look for.
     *
     * @return A list of clients whose IPID match. List may be empty.
     */
    QList<AOClient *> clientsByIpid(QString ipid);

    /**
     * @brief Gets a list of pointers to all clients with the given HWID.
     *
     * @param HWID The HWID to look for.
     *
     * @return A list of clients whose HWID match. List may be empty.
     */
    QList<AOClient *> clientsByHwid(QString f_hwid);

    /**
     * @brief Gets a pointer to a client by user ID.
     *
     * @param id The user ID to look for.
     *
     * @return A pointer to the client if found, a nullpointer if not.
     */
    AOClient *clientById(int id);

    /**
     * @brief Returns the overall player count in the server.
     *
     * @return The overall player count in the server.
     */
    int playerCount();

    /**
     * @brief Returns a list of the available characters on the server to use.
     *
     * @return A list of the available characters on the server to use.
     */
    QStringList characters();

    /**
     * @brief Returns the count of available characters on the server to use.
     *
     * @return The count of available characters on the server to use.
     */
    int characterCount();

    /**
     * @brief Get the available character by index.
     *
     * @param f_chr_id The index of the character.
     *
     * @return The character if it exist, otherwise an empty stirng.
     */
    QString characterById(int f_chr_id);

    /**
     * @brief Updates which characters are taken in the given area, and sends out an update packet to
     * all clients present the area.
     *
     * @param area The area in which to update the list of characters.
     */
    void updateCharsTaken(AreaData *area);

    /**
     * @brief Sends a packet to all clients in a given area.
     *
     * @param packet The packet to send to the clients.
     *
     * @param area_index The index of the area to look for clients in.
     *
     * @note Does nothing if an area by the given index does not exist.
     */
    void broadcast(const akashi::Packet &packet, int area_index);

    /**
     * @brief Sends a packet to all clients in the server.
     *
     * @param packet The packet to send to the clients.
     */
    void broadcast(const akashi::Packet &packet);

    /**
     * @brief Sends a packet to a specific usergroup..
     *
     * @param The packet to send to the clients.
     *
     * @param ENUM to determine the targets of the altered packet.
     */
    void broadcast(const akashi::Packet &packet, TARGET_TYPE target);

    /**
     * @brief Sends a packet to clients, sends an altered packet to a specific usergroup.
     *
     * @param The packet to send to the clients.
     *
     * @param The altered packet to send to the other clients.
     *
     * @param ENUM to determine the targets of the altered packet.
     */
    void broadcast(const akashi::Packet &packet, const akashi::Packet &other_packet, enum TARGET_TYPE target);

    /**
     * @brief Sends a packet to a single client.
     *
     * @param The packet send to the client.
     *
     * @param The temporary userID of the client.
     */
    void unicast(const akashi::Packet &f_packet, int f_client_id);

    /**
     * @brief Returns the character's character ID (= their index in the character list).
     *
     * @param char_name The 'internal' name for the character whose character ID to look up. This is equivalent to
     * the name of the directory of the character.
     *
     * @return The character ID if a character with that name exists in the character selection list, `-1` if not.
     */
    int characterId(QString char_name);

    /**
     * @brief Checks if an IP is in a subnet of the IPBanlist.
     **/
    bool isIPBanned(QHostAddress f_remote_IP);

    /**
     * @brief Returns the list of areas in the server.
     *
     * @return A list of areas.
     */
    QVector<AreaData *> areas();

    /**
     * @brief Returns the number of areas in the server.
     */
    int areaCount();

    /**
     * @brief Returns a pointer to the area associated with the index.
     *
     * @param f_area_id The index of the area.
     *
     * @return A pointer to the area or null.
     */
    AreaData *areaById(int f_area_id);

    /**
     * @brief Getter for an area specific buffer from the logger.
     */

    akashi::LogService *logService();
    akashi::EventBus *eventBus();
    void flushModcallLog(const QString &f_area_name);

    /**
     * @brief The names of the areas on the server.
     *
     * @return A list of names.
     */
    QStringList areaNames();

    /**
     * @brief Returns the name of the area associated with the index.
     *
     * @param f_area_id The index of the area.
     *
     * @return The name of the area or empty.
     */
    QString areaName(int f_area_id);

    /**
     * @brief Returns the available songs on the server.
     *
     * @return A list of songs.
     */
    QStringList musicList();

    /**
     * @brief Returns the available backgrounds on the server.
     *
     * @return A list of backgrounds.
     */
    QStringList backgrounds();

    /**
     * @brief Returns a pointer to a database manager.
     *
     * @return A pointer to a database manager.
     */
    DBManager *databaseManager();

    /**
     * @brief Returns the filesystem guard used for safe file access.
     */
    akashi::FileSystemService *fileSystem();

    /**
     * @brief Returns the registry of shared services.
     */
    akashi::ServiceRegistry *services();

    /**
     * @brief Returns the packet pipeline with its handlers and codecs.
     */
    std::shared_ptr<akashi::PacketService> packets();

    /**
     * @brief Returns a pointer to the server's Ye Olde Chat Filter
     */
    MedievalParser *medievalParser();

    /**
     * @brief Returns a pointer to ACL role handler.
     */
    ACLRolesHandler *aclRolesHandler();


    /**
     * @brief The server-wide global timer.
     */
    QTimer *timer;

    QStringList cursedCharsTaken(AOClient *client, QStringList chars_taken);

    /**
     * @brief Returns whatever a game message may be broadcasted or not.
     *
     * @return True if expired; false otherwise.
     */
    bool isMessageAllowed() const;

    /**
     * @brief Starts a global timer that determines whatever a game message may be broadcasted or not.
     *
     * @param f_duration The duration of the message floodguard timer.
     */
    void startMessageFloodguard(int f_duration);

    /**
     * @brief Attempts to parse a IPv6 mapped IPv4 to an IPv4.
     */
    QHostAddress parseToIPv4(QHostAddress f_remote_ip);

    /**
     * @brief Returns a raw-pointer of the curr
     */
    PlayerStateObserver *playerStateObserver();

    akashi::ArupBroadcaster *arupBroadcaster();

    akashi::CommandRegistry *commandRegistry();

    akashi::PermissionRegistry *permissionRegistry();

    akashi::TextFilterRegistry *textFilterRegistry();

    akashi::AuthThrottle *authThrottle();

    akashi::ConfigStore *configStore();
    ServerSettings *serverSettings();
    QString configPath(const QString &f_file) const;
    QString serverNickname() const;
    QUrl assetUrl() const;
    DataTypes::AuthType authType() const;
    QString loggingMode() const;

    void setMotd(const QString &f_motd);
    void setAuthType(DataTypes::AuthType f_auth);

    QStringList gimpList() const;
    QStringList filterList() const;
    QStringList cdnList() const;
    QStringList praiseList() const;
    QStringList reprimandsList() const;
    QStringList magic8BallAnswers() const;
    QStringList diceFaces(const QString &f_name) const;

  public Q_SLOTS:
    /**
     * @brief Convenience class to call a reload of available configuraiton elements.
     */
    void reloadSettings();

    /**
     * @brief Handles a new connection.
     *
     * @details The function creates an AOClient to represent the user, assigns a user ID to them, and
     * checks if the client is banned.
     */
    void clientConnected();

  Q_SIGNALS:

    /**
     * @brief This signal is emitted whenever the current player count has changed.
     *
     * @param f_current_player The player count at the time the signal was emitted.
     */
    void playerCountUpdated(int f_current_players);

  private:
    /**
     * @brief Listens for incoming websocket connections.
     */
    QWebSocketServer *server;

    /**
     * @brief Handles Discord webhooks.
     */
    Discord *discord;

    /**
     * @brief Handles HTTP server advertising.
     */
    ServerPublisher *server_publisher;

    akashi::LogService *m_log_service = nullptr;
    akashi::EventBus *m_event_bus = nullptr;
    std::shared_ptr<akashi::WriterText> m_text_writer;


    akashi::ConfigStore *m_config_store = nullptr;
    ServerSettings *m_server_settings = nullptr;
    DiscordSettings *m_discord_settings = nullptr;
    QSettings *m_areas_ini = nullptr;
    QSettings *m_ambience_ini = nullptr;

    struct TextData
    {
        QHash<QString, QStringList> dice_faces;
        QStringList magic_8ball;
        QStringList praises;
        QStringList reprimands;
        QStringList gimps;
        QStringList filters;
        QList<QRegularExpression> compiled_filters;
        QStringList cdns;
    };
    TextData m_text_data;

    int m_port;

    /**
     * @brief Every connected client and the ID pool, kept together so they
     * can never disagree. When no ID is free the server rejects new
     * connection attempts.
     */
    PlayerDirectory m_player_directory;

    akashi::ArupBroadcaster *m_arup_broadcaster = nullptr;
    akashi::CommandRegistry *m_command_registry = nullptr;
    akashi::PermissionRegistry *m_permission_registry = nullptr;
    akashi::AuthThrottle *m_auth_throttle = nullptr;
    akashi::TextFilterRegistry *m_text_filter_registry = nullptr;

    PlayerStateObserver m_player_state_observer;

    /**
     * @brief The overall player count in the server.
     */
    int m_player_count;

    /**
     * @brief The characters available on the server to use.
     */
    QStringList m_characters;

    /**
     * @brief The areas on the server.
     */
    QVector<AreaData *> m_areas;

    /**
     * @brief The names of the areas on the server.
     *
     * @details Equivalent to iterating over #areas and getting the area names individually, but grouped together
     * here for faster access.
     */
    QStringList m_area_names;


    /**
     * @brief The default floor holding the music catalog from music.json.
     */
    akashi::Floor m_default_floor;

    /**
     * @brief The backgrounds on the server that may be used in areas.
     */
    QStringList m_backgrounds;

    /**
     * @brief Collection of all IPs that are banned.
     */
    QStringList m_ipban_list;

    // The ASNs that are banned and the database to look up who announces an address.
    QList<quint32> m_banned_asns;
    akashi::MmdbReader m_asn_reader;

    /**
     * @brief Timer until the next IC message can be sent.
     */
    QTimer *m_message_floodguard_timer;

    /**
     * @brief If false, IC messages will be rejected.
     */
    bool m_can_send_ic_messages = true;

    /**
     * @brief The database manager on the server, used to store users' bans and authorisation details.
     */
    DBManager *db_manager;

    /**
     * @brief The filesystem guard, resolved from the service registry.
     */
    akashi::FileSystemService *m_filesystem = nullptr;

    /**
     * @brief The registry of shared services, owned by the server context.
     */
    akashi::ServiceRegistry *m_services;

    /**
     * @brief The packet pipeline, resolved from the service registry.
     */
    std::shared_ptr<akashi::PacketService> m_packets;

    /**
     * @brief Medieval mode text parser class
     */
    MedievalParser *medieval_parser;

    /**
     * @see ACLRolesHandler
     */
    ACLRolesHandler *acl_roles_handler;


    /**
     * @brief Takes a client out of the server: player count, roster, its
     * presence in areas, and finally the object itself.
     */
    void removeClient(AOClient *f_client);

    /**
     * @brief Hands the configured ID assignment style to the player
     * directory; called at startup and whenever the settings reload.
     */
    void applyIdAssignment();
    void reloadTextData();
    void reloadMusicFloor();
    void reloadBanLists();


  private Q_SLOTS:
    /**
     * @brief Increase the current player count by one.
     */
    void increasePlayerCount();

    /**
     * @brief Decrease the current player count based on the client id provided.
     *
     * @param f_client_id The client id of the client to check.
     */
    void decreasePlayerCount();

    /**
     * @brief Allow game messages to be broadcasted.
     */
    void allowMessage();
};

