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
#include "core/player_directory.h"
#include "core/player_state_observer.h"
#include "core/server_settings.h"
#include "proto/packet.h"
#include "world/floodguard.h"
#include "world/floor.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QHostAddress>
#include <QMap>
#include <QPointer>
#include <QRegularExpression>
#include <QSettings>
#include <QStack>
#include <QString>
#include <QTimer>
#include <QUrl>

#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace akashi {
class Area;
class World;
class ITransport;
enum class DisconnectKind;
struct JukeboxSong;
struct PermissionQuery;
enum class PermissionVerdict;
class WebSocketReceiver;
class PluginManager;
class ConsoleMenu;
class ArupBroadcaster;
class Authenticator;
class AuthenticatorRegistry;
struct AuthOutcome;
class AuthThrottle;
class CommandRegistry;
class ConfigStore;
class DatabaseService;
class FileSystemService;
class LogService;
class DiscordHook;
class PacketService;
class RuleRegistry;
class Scheduler;
class TextFilterRegistry;
class WriterText;
class PermissionRegistry;
class ServiceRegistry;
}

class ServerPublisher;
namespace akashi {
class ACLRolesHandler;
class ClientSession;
}
class DBManager;
class MedievalParser;
/**
 * @brief The server: its lifecycle, its world, its people and its doors.
 */
class AKASHI_CORE_EXPORT ServerContext : public QObject
{
    Q_OBJECT

  public:
    enum class Stage
    {
        Configuring,
        ServicesConstructed,
        ContentLoaded,
        PluginsLoaded,
        Listening,
        Running,
        Draining,
        Stopped,
    };
    Q_ENUM(Stage)

    explicit ServerContext(QObject *parent = nullptr);

    // Runs shutdown() as a safety net; normally it is called explicitly.
    ~ServerContext();

    // Builds and starts the server. Returns the exit code describing why
    // startup failed.
    ExitCode start();

    // Stops the server. Safe to call more than once.
    void shutdown();

    Stage stage() const;

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
    QVector<akashi::ClientSession *> clients() const;

    /**
     * @brief Gets a list of pointers to all clients with the given IPID.
     *
     * @param ipid The IPID to look for.
     *
     * @return A list of clients whose IPID match. List may be empty.
     */
    QList<akashi::ClientSession *> clientsByIpid(QString ipid) const;

    /**
     * @brief Gets a list of pointers to all clients with the given HWID.
     *
     * @param HWID The HWID to look for.
     *
     * @return A list of clients whose HWID match. List may be empty.
     */
    QList<akashi::ClientSession *> clientsByHwid(QString f_hwid) const;

    /**
     * @brief Gets a pointer to a client by user ID.
     *
     * @param id The user ID to look for.
     *
     * @return A pointer to the client if found, a nullpointer if not.
     */
    akashi::ClientSession *clientById(int id) const;

    /**
     * @brief Returns the overall player count in the server.
     *
     * @return The overall player count in the server.
     */
    int playerCount() const;

    /**
     * @brief Returns a list of the available characters on the server to use.
     *
     * @return A list of the available characters on the server to use.
     */
    QStringList characters() const;

    /**
     * @brief Returns the count of available characters on the server to use.
     *
     * @return The count of available characters on the server to use.
     */
    int characterCount() const;

    /**
     * @brief Get the available character by index.
     *
     * @param f_chr_id The index of the character.
     *
     * @return The character if it exist, otherwise an empty stirng.
     */
    QString characterById(int f_chr_id) const;

    /**
     * @brief Updates which characters are taken in the given area, and sends out an update packet to
     * all clients present the area.
     *
     * @param area The area in which to update the list of characters.
     */
    void updateCharsTaken(akashi::Area *area);

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
     * @brief Sends one finished IC message to an area. The one place an
     * outgoing MS packet is built.
     *
     * @param f_fields The classic positional MS fields.
     *
     * @param f_area_id The area whose clients receive the message.
     *
     * @param f_viewer_fields When set, resolves the fields each viewer
     * sees (the hidden-CM evidence index remap); otherwise everyone gets
     * f_fields.
     */
    void broadcastIc(const QStringList &f_fields, int f_area_id, const std::function<QStringList(akashi::ClientSession *)> &f_viewer_fields = {});

    /**
     * @brief The kick verb: every listed client is told the reason with a
     * KK packet and dropped, and one log entry records the act. The doors
     * (/kick, /kick_uid and the MA packet) resolve their targets and
     * phrase their own replies.
     */
    void kickClients(const QList<akashi::ClientSession *> &f_targets, const QString &f_target_ipid, const QString &f_reason, const QString &f_moderator);

    struct BanResult
    {
        int ban_id = -1;
        int clients_kicked = 0;
    };

    /**
     * @brief The ban verb: writes one ban row per distinct hardware id
     * among the online clients with the IPID (or a bare row when nobody
     * is online), sends each client the door's KB notice, logs once and
     * publishes the one BanIssuedEvent.
     *
     * @param f_duration_secs Ban length in seconds; -2 is permanent.
     *
     * @param f_perma_text The door's own spelling of "forever" for the
     * log, the event and the notice.
     *
     * @param f_notice Builds the KB text from the ban id and the until
     * text; empty means the bare reason.
     */
    BanResult banPlayers(const QString &f_ipid, long long f_duration_secs, const QString &f_reason, const QString &f_moderator, const QString &f_perma_text, const std::function<QString(int f_ban_id, const QString &f_until)> &f_notice = {});

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
    int characterId(QString char_name) const;

    /**
     * @brief Checks if an IP is in a subnet of the IPBanlist.
     **/
    bool isIPBanned(QHostAddress f_remote_IP);

    /**
     * @brief Returns the list of areas in the server.
     *
     * @return A list of areas.
     */
    QVector<akashi::Area *> areas() const;

    /**
     * @brief Returns the number of areas in the server.
     */
    int areaCount() const;

    /**
     * @brief Returns a pointer to the area associated with the index.
     *
     * @param f_area_id The index of the area.
     *
     * @return A pointer to the area or null.
     */
    akashi::Area *areaById(int f_area_id) const;

    /**
     * @brief Getter for an area specific buffer from the logger.
     */

    akashi::LogService *logService() const;

    /**
     * @brief Publishes a placeless event straight to the global observers:
     * a serverwide fact with no area, so no rule phase dispatches. The two
     * ids name the acting user slot and the session behind it; both stay
     * -1 for actor-less facts like config reloads.
     */
    void publishEvent(const QString &f_id, const QVariantMap &f_payload, int f_player_state_id = -1, int f_client_session_id = -1);

    void flushModcallLog(const QString &f_area_name);

    /**
     * @brief The names of the areas on the server.
     *
     * @return A list of names.
     */
    QStringList areaNames() const;

    /**
     * @brief Returns the name of the area associated with the index.
     *
     * @param f_area_id The index of the area.
     *
     * @return The name of the area or empty.
     */
    QString areaName(int f_area_id) const;

    int floorCount() const;
    const akashi::Floor *floorById(int f_floor_id) const;
    akashi::Floor *floorById(int f_floor_id);
    const akashi::Floor *floorByName(const QString &f_name) const;
    int floorIdForArea(int f_area_id) const;
    QStringList floorNames() const;

    /**
     * @brief Replaces all "config"-owned rules on floors and areas with the
     * declarations currently in areas.json. Runs at startup and on
     * /reloadrules; a plain settings reload leaves rules alone so runtime
     * rule changes survive it.
     */
    void applyConfigRules();

    /**
     * @brief Creates a new area on the given floor and catches the floor's
     * clients up on their new area list.
     *
     * @return The new area's ID, or -1 if the floor or name is invalid.
     */
    int createArea(const QString &f_name, int f_floor_id);

    /**
     * @brief Creates a new floor holding one starter area, with the core
     * rule set applied.
     *
     * @return The new floor's ID, or -1 if the name is taken or empty.
     */
    int createFloor(const QString &f_name);

    bool renameArea(int f_area_id, const QString &f_name);
    bool renameFloor(int f_floor_id, const QString &f_name);

    /**
     * @brief Removes an empty area. The id space is compacted; every later
     * area, its clients, and the floor's area lists renumber in one sweep.
     *
     * @return The reason it was refused, or nothing on success.
     */
    std::optional<QString> removeArea(int f_area_id);

    /**
     * @brief Removes a floor whose areas are all empty, areas included.
     */
    std::optional<QString> removeFloor(int f_floor_id);

    /**
     * @brief Writes the live world back to areas.json: floors, area
     * settings, and the rules that config and commands put there. Core
     * defaults and plugin rules stay out of the file.
     *
     * @return The reason the save was refused, or nothing on success.
     */
    std::optional<QString> saveWorld();

    /**
     * @brief Rebuilds every floor and area from areas.json. Players keep
     * their place by area name where possible and receive the full join
     * delivery for the rebuilt world. Session-only area state resets.
     */
    std::optional<QString> reloadWorld();

    /**
     * @brief Writes one floor - its rules, areas and their settings - to
     * config/floors/<name>.json in the same shape as areas.json.
     */
    std::optional<QString> saveFloor(int f_floor_id);

    /**
     * @brief Loads a floor file. A floor of the same name is replaced in
     * place (players keep their spot by area name, or land in the floor's
     * first area); an unknown name becomes a new floor.
     */
    std::optional<QString> loadFloor(const QString &f_name);

    QStringList musicList() const;

    /**
     * @brief Returns the available backgrounds on the server.
     *
     * @return A list of backgrounds.
     */
    QStringList backgrounds() const;

    /**
     * @brief Returns a pointer to a database manager.
     *
     * @return A pointer to a database manager.
     */
    DBManager *databaseManager() const;

    /**
     * @brief Returns the filesystem guard used for safe file access.
     */
    akashi::FileSystemService *fileSystem() const;

    /**
     * @brief Returns the registry of shared services.
     */
    akashi::ServiceRegistry *services() const;

    /**
     * @brief Returns the packet pipeline with its handlers and codecs.
     */
    std::shared_ptr<akashi::PacketService> packets() const;

    /**
     * @brief Returns a pointer to ACL role handler.
     */
    akashi::ACLRolesHandler *aclRolesHandler() const;

    /**
     * @brief The server-wide global timer.
     */
    QTimer *timer = nullptr;

    QStringList cursedCharsTaken(akashi::ClientSession *client, QStringList chars_taken) const;

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
    QHostAddress parseToIPv4(QHostAddress f_remote_ip) const;

    /**
     * @brief Returns a raw-pointer of the curr
     */
    PlayerStateObserver *playerStateObserver();

    akashi::ArupBroadcaster *arupBroadcaster() const;

    akashi::ConsoleMenu *consoleMenu() const;

    akashi::CommandRegistry *commandRegistry() const;

    akashi::PermissionRegistry *permissionRegistry() const;

    akashi::TextFilterRegistry *textFilterRegistry() const;

    akashi::RuleRegistry *ruleRegistry() const;

    akashi::World *world() const;

    akashi::AuthThrottle *authThrottle() const;

    akashi::AuthenticatorRegistry *authenticatorRegistry();

    // The single authentication system resolved at launch, and its id.
    // Read-only for the server's whole life: /reload never changes it.
    std::shared_ptr<akashi::Authenticator> activeAuthenticator() const;
    QString activeAuthSystemId() const;

    // Hands an authentication outcome to the session that asked, if it
    // still exists. Client ids get reused, so delivery goes through the
    // QPointer and never through an id lookup.
    void deliverAuthOutcome(QPointer<akashi::ClientSession> f_session, const akashi::AuthOutcome &f_outcome);

    akashi::ConfigStore *configStore() const;
    ServerSettings *serverSettings() const;
    QString configPath(const QString &f_file) const;
    QString serverNickname() const;
    QUrl assetUrl() const;
    AuthType authType() const;
    QString loggingMode() const;

    void setMotd(const QString &f_motd);

    QStringList gimpList() const;
    QStringList cdnList() const;
    QStringList praiseList() const;
    QStringList reprimandsList() const;
    QStringList magic8BallAnswers() const;
    QStringList diceFaces(const QString &f_name) const;

    /**
     * @brief Applies a sanction that outlives the session: the flag on
     * every online client with the IPID, a row in the database, and a
     * scheduled lift.
     */
    void applyTimedSanction(const QString &f_ipid, const QString &f_sanction, const QDateTime &f_until, const QString &f_moderator);

    /**
     * @brief Drops the stored row and the pending lift of a sanction, for
     * when a moderator lifts it by hand or makes it permanent. Session
     * flags stay with the caller.
     */
    void clearStoredSanction(const QString &f_ipid, const QString &f_sanction);

  public Q_SLOTS:
    /**
     * @brief Convenience class to call a reload of available configuraiton elements.
     */
    void reloadSettings();

    /**
     * @brief Sweeps an unloading plugin's rules off every floor and area:
     * entries the plugin attached itself, and entries built from its
     * actions. Runs before the plugin's library leaves memory.
     */
    void onPluginAboutToUnload(const QString &f_plugin_id);

    /**
     * @brief Handles a new connection.
     *
     * @details The function creates an akashi::ClientSession to represent the user, assigns a user ID to them, and
     * checks if the client is banned.
     */
    void inboundClient(akashi::ITransport *f_transport);

  Q_SIGNALS:

    /**
     * @brief This signal is emitted whenever the current player count has changed.
     *
     * @param f_current_player The player count at the time the signal was emitted.
     */
    void playerCountUpdated(int f_current_players);

    void stageChanged(ServerContext::Stage f_stage);

  private:
    /**
     * @brief The WebSocket door clients come through. Plugin receivers for
     * other protocols connect their inboundClient signal to the same slot.
     */
    akashi::WebSocketReceiver *m_receiver = nullptr;

    akashi::DatabaseService *m_database_service = nullptr;
    akashi::Scheduler *m_scheduler = nullptr;
    akashi::DiscordHook *m_discord_hook = nullptr;
    akashi::PluginManager *m_plugin_manager = nullptr;
    Stage m_stage = Stage::Configuring;

    void setStage(Stage f_stage);

    /**
     * @brief Constructs the world, the registries and the resources once
     * the configuration is verified.
     */
    void buildCore();

    /**
     * @brief Puts the core's own entries on the console menu's tasks view:
     * the scheduled maintenance and backup jobs plus common operator tasks.
     */
    void registerConsoleTasks();

    // The scheduled database jobs; the postpone check is bound to the
    // configured player ceiling.
    void runDatabaseMaintenance();
    void runDatabaseBackups();
    bool isMaintenanceBusy(int f_max_players);

    // The console tasks view: when a job next fires, and the operator's
    // run-now and inspection tasks.
    std::optional<QDateTime> nextScheduledRun(const QString &f_job_id) const;
    void runMaintenanceAndReport();
    void runBackupsAndReport();
    void showRecentBans();
    void showDatabaseOverview();

    /**
     * @brief Arms a lift job for every stored sanction at boot; overdue
     * ones fire on the first tick and clean their rows up.
     */
    void armStoredSanctions();

    /**
     * @brief Applies the stored sanctions of a connecting client's IPID.
     */
    void applyStoredSanctions(akashi::ClientSession *f_client);

    /**
     * @brief Schedules the lift moment of a stored sanction.
     */
    void scheduleSanctionLift(const QString &f_ipid, const QString &f_sanction, const QDateTime &f_until);

    /**
     * @brief Drops a sanction's row and session flags once its moment
     * passes; the scheduler runs it bound to the ipid and sanction.
     */
    void liftSanction(const QString &f_ipid, const QString &f_sanction);

    /**
     * @brief Opens the receiver and assembles everything that needs a
     * listening server: the publisher, the world, the ban lists.
     */
    ExitCode startListening();

    // The built-in permission resolver chain.
    static akashi::PermissionVerdict resolveNoneCheck(const akashi::PermissionQuery &f_query);
    akashi::PermissionVerdict resolveJoinedUser(const akashi::PermissionQuery &f_query);
    akashi::PermissionVerdict resolveAreaOwner(const akashi::PermissionQuery &f_query);
    static akashi::PermissionVerdict resolveAuthentication(const akashi::PermissionQuery &f_query);
    akashi::PermissionVerdict resolveRoleCheck(const akashi::PermissionQuery &f_query);

    // The core text filters.
    std::optional<QString> applyWordFilter(const QString &f_text) const;
    std::optional<QString> applyGimpFilter(const QString &f_text) const;
    std::optional<QString> applyMedievalFilter(const QString &f_text) const;
    static std::optional<QString> applyShakeFilter(const QString &f_text);
    static std::optional<QString> applyDisemvowelFilter(const QString &f_text);

    // Resolves an area owner to "[id] character" for the ARUP CM column.
    QString formatAreaOwner(int f_owner_id);

    // Signal reactions bound to their subject at connect time, so their
    // signatures carry more than the signal's arguments.
    void onClientTransportClosed(akashi::ClientSession *f_client, akashi::DisconnectKind f_kind);
    void onJukeboxMusicListChanged(int f_area_id, akashi::Area *f_area);
    void onJukeboxSongStarted(akashi::Area *f_area, const akashi::JukeboxSong &f_song);

    /**
     * @brief Handles HTTP server advertising.
     */
    ServerPublisher *server_publisher = nullptr;

    akashi::LogService *m_log_service = nullptr;
    std::shared_ptr<akashi::WriterText> m_text_writer;
    std::unique_ptr<MedievalParser> m_medieval_parser;

    akashi::ConfigStore *m_config_store = nullptr;
    ServerSettings *m_server_settings = nullptr;
    QSettings *m_areas_ini = nullptr;

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

    int m_port = 0;

    /**
     * @brief Every connected client and the ID pool, kept together so they
     * can never disagree. When no ID is free the server rejects new
     * connection attempts.
     */
    akashi::PlayerDirectory m_player_directory;

    akashi::ArupBroadcaster *m_arup_broadcaster = nullptr;
    akashi::CommandRegistry *m_command_registry = nullptr;
    akashi::ConsoleMenu *m_console_menu = nullptr;
    akashi::PermissionRegistry *m_permission_registry = nullptr;
    akashi::AuthThrottle *m_auth_throttle = nullptr;
    akashi::RuleRegistry *m_rule_registry = nullptr;
    akashi::TextFilterRegistry *m_text_filter_registry = nullptr;
    akashi::AuthenticatorRegistry *m_authenticator_registry = nullptr;

    // The one active authentication system, resolved once at launch.
    std::shared_ptr<akashi::Authenticator> m_active_authenticator;
    QString m_active_auth_system_id;

    PlayerStateObserver m_player_state_observer;

    /**
     * @brief The overall player count in the server.
     */
    int m_player_count = 0;

    /**
     * @brief The characters available on the server to use.
     */
    QStringList m_characters;

    /**
     * @brief The backgrounds on the server that may be used in areas.
     */
    QStringList m_backgrounds;

    /**
     * @brief The banned IP ranges, parsed once at (re)load - the check runs on every connection.
     */
    QList<std::pair<QHostAddress, int>> m_ipban_subnets;

    // The ASNs that are banned and the database to look up who announces an address.
    QList<quint32> m_banned_asns;
    akashi::MmdbReader m_asn_reader;

    /**
     * @brief The server-wide IC rate gate; areas each hold their own.
     */
    akashi::Floodguard m_message_floodguard;

    /**
     * @brief The database manager on the server, used to store users' bans and authorisation details.
     */
    DBManager *db_manager = nullptr;

    /**
     * @brief The filesystem guard, resolved from the service registry.
     */
    akashi::FileSystemService *m_filesystem = nullptr;

    /**
     * @brief The registry of shared services, owned by the server context.
     */
    akashi::ServiceRegistry *m_services = nullptr;

    /**
     * @brief The packet pipeline, resolved from the service registry.
     */
    std::shared_ptr<akashi::PacketService> m_packets;

    /**
     * @see ACLRolesHandler
     */
    akashi::ACLRolesHandler *acl_roles_handler = nullptr;

    /**
     * @brief Takes a client out of the server: player count, roster, its
     * presence in areas, and finally the object itself.
     */
    void removeClient(akashi::ClientSession *f_client);

    /**
     * @brief Hands the configured ID assignment style to the player
     * directory; called at startup and whenever the settings reload.
     */
    void applyIdAssignment();
    void reloadTextData();
    void reloadMusicFloor();
    void reloadBanLists();
    void reloadAclRoles();
    void reloadAuthLimits();

    // The auth system is chosen at launch and read-only afterwards; a
    // reload that names a different system only earns a warning.
    void warnOnAuthSystemChange();

    // Maps the auth setting to a system id: the historic values name the
    // two core systems, anything else is a plugin-provided system id.
    QString configuredAuthSystemId() const;

    // Resolves the single active authenticator once plugins have started.
    // Fails loud: a configured id nobody provides aborts startup.
    bool resolveActiveAuthenticator();

    // The random secret both bootstraps hand out: hex, sized by the
    // configured minimum password length.
    QString generateBootstrapSecret() const;

    // Creates the root account with a one-time random password when the
    // username system boots over a database that has none.
    void bootstrapRootAccount();

    // Generates and persists a random modpass when the password system
    // boots with an empty one.
    void bootstrapModpass();

    /**
     * @brief Tells the global observers the config reloaded, after core
     * re-read everything.
     */
    void publishConfigReloaded();

    /**
     * @brief Sends a fresh area list and full ARUP to everyone on a floor.
     */
    void refreshFloorClients(int f_floor_id);

    /**
     * @brief Moves every client's area number with a world reshaping: the
     * mapping is old id to new id, -1 for a removed area.
     */
    void applyAreaMapping(const QVector<int> &f_mapping);

    /**
     * @brief The map: floors, areas, their config and reshaping.
     */
    akashi::World *m_world = nullptr;

  private Q_SLOTS:
    /**
     * @brief Re-applies the command extensions once a runtime-loaded
     * plugin has registered its commands.
     */
    void onPluginLoaded();

    /**
     * @brief Sends one ARUP packet to every area on the floor.
     */
    void onArupFloorBroadcast(const akashi::Packet &f_packet, int f_floor_id);

    /**
     * @brief Wires a newly built area: its jukebox packets and the
     * area-update broadcaster.
     */
    void onAreaBuilt(akashi::Area *f_area);

    /**
     * @brief Drops a dying area from the area-update broadcaster.
     */
    void onAreaAboutToBeRemoved(akashi::Area *f_area);

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
};
