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
#ifndef AOCLIENT_H
#define AOCLIENT_H

#include <algorithm>

#include <QDateTime>
#include <QHostAddress>
#include <QRegularExpression>
#include <QTcpSocket>
#include <QtGlobal>
#include <QTimer>
#if QT_VERSION > QT_VERSION_CHECK(5, 10, 0)
#include <QRandomGenerator>
#endif

#include "include/aopacket.h"

class AreaData;
class DBManager;
class MusicManager;
class Server;

/**
 * @brief Represents a client connected to the server running Attorney Online 2 or one of its derivatives.
 */
class AOClient : public QObject
{
    Q_OBJECT

  public:
    /**
     * @brief Creates an instance of the AOClient class.
     *
     * @param p_server A pointer to the Server instance where the client is joining to.
     * @param p_socket The socket associated with the AOClient.
     * @param user_id The user ID of the client.
     * @param parent Qt-based parent, passed along to inherited constructor from QObject.
     */
    AOClient(Server *p_server, QTcpSocket *p_socket, QObject *parent = nullptr, int user_id = 0, MusicManager *p_manager = nullptr);

    /**
     * @brief Destructor for the AOClient instance.
     *
     * @details Sets the socket to delete later.
     */
    ~AOClient();

    /**
     * @brief Getter for the client's IPID.
     *
     * @return The IPID.
     *
     * @see #ipid
     */
    QString getIpid() const;

    /**
     * @brief Getter for the client's HWID.
     *
     * @return The HWID.
     *
     * @see #hwid
     */
    QString getHwid() const;

    /**
     * @brief Returns true if the client has completed the participation handshake. False otherwise.
     *
     * @return True if the client has completed the participation handshake. False otherwise.
     */
    bool hasJoined() const;

    /**
     * @brief Calculates the client's IPID based on a hashed version of its IP.
     */
    void calculateIpid();

    /**
     * @brief Getter for the pointer to the server.
     *
     * @return See brief description.
     *
     * @note Unused. There isn't really a point to this existing, either.
     *
     * @see #server
     */
    Server *getServer();

    /**
     * @brief The user ID of the client.
     */
    int m_id;

    /**
     * @brief The IP address of the client.
     */
    QHostAddress m_remote_ip;

    /**
     * @brief The stored character password for the client, used to be able to select passworded characters.
     */
    QString m_password;

    /**
     * @brief True if the client is actually in the server.
     *
     * @details To explain: In AO, clients immediately establish connection to the server when the user clicks on the server's name in the server
     * browser. Thus, as the user browses servers, they constantly connect and disconnect to and from servers.
     *
     * The purpose of this variable is to determine if the user isn't just doing that, but has actually double-clicked the server, and
     * its client has sent the standard handshake packets, which does signify that the client intended to 'join' this server.
     */
    bool m_joined;

    /**
     * @brief The ID of the area the client is currently in.
     */
    int m_current_area;

    /**
     * @brief The internal name of the character the client is currently using.
     */
    QString m_current_char;

    /**
     * @brief The internal name of the character the client is iniswapped to.
     *
     * @note This will be the same as current_char if the client is not iniswapped.
     */
    QString m_current_iniswap;

    /**
     * @brief If true, the client is a logged-in moderator.
     */
    bool m_authenticated = false;

    /**
     * @brief If using advanced authentication, this is the moderator name that the client has logged in with.
     */
    QString m_moderator_name = "";

    /**
     * @brief The out-of-character name of the client, generally the nickname of the user themself.
     */
    QString m_ooc_name = "";

    /**
     * @brief The custom showname of the client, used when "renaming" already existing characters in-character.
     */
    QString m_showname = "";

    /**
     * @brief If true, the client is willing to receive global messages.
     *
     * @see AOClient::cmdG and AOClient::cmdToggleGlobal
     */
    bool m_global_enabled = true;

    /**
     * @brief If true, the client's messages will be sent in first-person mode.
     *
     * @see AOClient::cmdFirstPerson
     */
    bool m_first_person = false;

    /**
     * @brief If true, the client may not use in-character chat.
     */
    bool m_is_muted = false;

    /**
     * @brief If true, the client may not use out-of-character chat.
     */
    bool m_is_ooc_muted = false;

    /**
     * @brief If true, the client may not use the music list.
     */
    bool m_is_dj_blocked = false;

    /**
     * @brief If true, the client may not use the judge controls.
     */
    bool m_is_wtce_blocked = false;

    /**
     * @brief Represents the client's client software, and its version.
     *
     * @note Though the version number and naming scheme looks vaguely semver-like,
     * do not be misled into thinking it is that.
     */
    struct ClientVersion
    {
        QString string;   //!< The name of the client software, for example, `AO2`.
        int release = -1; //!< The 'release' part of the version number. In Attorney Online's case, this is fixed at `2`.
        int major = -1;   //!< The 'major' part of the version number. In Attorney Online's case, this increases when a new feature is introduced (generally).
        int minor = -1;   //!< The 'minor' part of the version number. In Attorney Online's case, this increases for bugfix releases (generally).
    };

    /**
     * @brief The software and version of the client.
     *
     * @see The struct itself for more details.
     */
    ClientVersion m_version;

    /**
     * @brief The authorisation bitflag, representing what permissions a client can have.
     *
     * @showinitializer
     */
    QMap<QString, unsigned long long> ACLFlags{
        {"NONE", 0ULL},
        {"KICK", 1ULL << 0},
        {"BAN", 1ULL << 1},
        {"BGLOCK", 1ULL << 2},
        {"MODIFY_USERS", 1ULL << 3},
        {"CM", 1ULL << 4},
        {"GLOBAL_TIMER", 1ULL << 5},
        {"EVI_MOD", 1ULL << 6},
        {"MOTD", 1ULL << 7},
        {"ANNOUNCE", 1ULL << 8},
        {"MODCHAT", 1ULL << 9},
        {"MUTE", 1ULL << 10},
        {"UNCM", 1ULL << 11},
        {"SAVETEST", 1ULL << 12},
        {"FORCE_CHARSELECT", 1ULL << 13},
        {"BYPASS_LOCKS", 1ULL << 14},
        {"IGNORE_BGLIST", 1ULL << 15},
        {"SEND_NOTICE", 1ULL << 16},
        {"JUKEBOX", 1ULL << 17},
        {"SUPER", ~0ULL}};

    /**
     * @brief A list of 5 casing preferences (def, pro, judge, jury, steno)
     */
    QList<bool> m_casing_preferences = {false, false, false, false, false};

    /**
     * @brief If true, the client's in-character messages will have their word order randomised.
     */
    bool m_is_shaken = false;

    /**
     * @brief If true, the client's in-character messages will have their vowels (English alphabet only) removed.
     */
    bool m_is_disemvoweled = false;

    /**
     * @brief If true, the client's in-character messages will be overwritten by a randomly picked predetermined message.
     */
    bool m_is_gimped = false;

    /**
     * @brief If true, the client will be marked as AFK in /getarea. Automatically applied when a configurable
     * amount of time has passed since the last interaction, or manually applied by /afk.
     */
    bool m_is_afk = false;

    /**
     * @brief If true, the client will not recieve PM messages.
     */
    bool m_pm_mute = false;

    /**
     * @brief If true, the client will recieve advertisements.
     */
    bool m_advert_enabled = true;

    /**
     * @brief If true, the client is restricted to only changing into certain characters.
     */
    bool m_is_charcursed = false;

    /**
     * @brief Timer for tracking user interaction. Automatically restarted whenever a user interacts (i.e. sends any packet besides CH)
     */
    QTimer *m_afk_timer;

    /**
     * @brief The list of char IDs a charcursed player is allowed to switch to.
     */
    QList<int> m_charcurse_list;

    /**
     * @brief Temporary client permission if client is allowed to save a testimony to server storage.
     */
    bool m_testimony_saving = false;

    /**
     * @brief If true, the client's next OOC message will be interpreted as a moderator login.
     */
    bool m_is_logging_in = false;

    /**
     * @brief Checks if the client would be authorised to something based on its necessary permissions.
     *
     * @param acl_mask The permissions bitflag that the client's own permissions should be checked against.
     *
     * @return True if the client's permissions are high enough for `acl_mask`, or higher than it.
     * False if the client is missing some permissions.
     */
    bool checkAuth(unsigned long long acl_mask);

  public slots:
    /**
     * @brief A slot for when the client disconnects from the server.
     */
    void clientDisconnected();

    /**
     * @brief A slot for when the client sends data to the server.
     */
    void clientData();

    /**
     * @brief A slot for sending a packet to the client.
     *
     * @param packet The packet to send.
     */
    void sendPacket(AOPacket packet);

    /**
     * @overload
     */
    void sendPacket(QString header, QStringList contents);

    /**
     * @overload
     */
    void sendPacket(QString header);

    /**
     * @brief A slot for when the client's AFK timer runs out.
     */
    void onAfkTimeout();

  signals:
    /**
     * @brief This signal is emitted when the client has completed the participation handshake.
     */
    void joined();

  private:
    /**
     * @brief The TCP socket used to communicate with the client.
     */
    QTcpSocket *m_socket;

    /**
     * @brief A pointer to the Server, used for updating server variables that depend on the client (e.g. amount of players in an area).
     */
    Server *server;

    /**
     * @brief The type of area update, used for area update (ARUP) packets.
     */
    enum ARUPType
    {
        PLAYER_COUNT, //!< The packet contains player count updates.
        STATUS,       //!< The packet contains area status updates.
        CM,           //!< The packet contains updates about who's the CM of what area.
        LOCKED        //!< The packet contains updates about what areas are locked.
    };

    /**
     * @brief Handles an incoming packet, checking for authorisation and minimum argument count.
     *
     * @param packet The incoming packet.
     */
    void handlePacket(AOPacket packet);

    /**
     * @brief Handles an incoming command, checking for authorisation and minimum argument count.
     *
     * @param command The incoming command.
     * @param argc The amount of arguments the command was called with. Equivalent to `argv.size()`.
     * @param argv The arguments the command was called with.
     */
    void handleCommand(QString command, int argc, QStringList argv);

    /**
     * @brief Changes the area the client is in.
     *
     * @param new_area The ID of the new area.
     */
    void changeArea(int new_area);

    /**
     * @brief Changes the client's character.
     *
     * @param char_id The character ID of the client's new character.
     */
    bool changeCharacter(int char_id);

    /**
     * @brief Changes the client's in-character position.
     *
     * @param new_pos The new position of the client.
     */
    void changePosition(QString new_pos);

    /**
     * @brief Sends or announces an ARUP update.
     *
     * @param type The type of ARUP to send.
     * @param broadcast If true, the update is sent out to all clients on the server. If false, it is only sent to this client.
     *
     * @see AOClient::ARUPType
     */
    void arup(ARUPType type, bool broadcast);

    /**
     * @brief Sends all four types of ARUP to the client.
     */
    void fullArup();
    /**
     * @brief Sends an out-of-character message originating from the server to the client.
     *
     * @param message The text of the message to send.
     */
    void sendServerMessage(QString message);

    /**
     * @brief Like with AOClient::sendServerMessage(), but to every client in the client's area.
     *
     * @param message The text of the message to send.
     */
    void sendServerMessageArea(QString message);

    /**
     * @brief Like with AOClient::sendServerMessage(), but to every client in the server.
     *
     * @param message The text of the message to send.
     */
    void sendServerBroadcast(QString message);

    /**
     * @name Packet headers
     *
     * @details These functions implement the AO2-style packet handling.
     * As these should generally be the same across server software, I see no reason to document them specifically.
     *
     * You can check out the AO2 network protocol for explanations.
     *
     * All packet handling functions share the same parameters:
     *
     * @param area The area the client is in. Some packets make use of the client's current area.
     * @param argc The amount of arguments in the packet, not counting the header. Same as `argv.size()`.
     * @param argv The arguments in the packet, once again, not counting the header.
     * @param packet The... arguments in the packet. Yes, exactly the same as `argv`, just packed into an AOPacket.
     *
     * @see https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md for the AO2 network protocol.
     */
    ///@{

    /// A "default" packet handler, to be used for error checking and copying other packet handlers.
    void pktDefault(AreaData *area, int argc, QStringList argv, AOPacket packet);

    /// Implements [hardware ID](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#hard-drive-id).
    void pktHardwareId(AreaData *area, int argc, QStringList argv, AOPacket packet);

    /**
     * @brief Implements [feature list](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#feature-list) and
     * [player count](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#player-count).
     */
    void pktSoftwareId(AreaData *area, int argc, QStringList argv, AOPacket packet);

    /// Implements [resource counts](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#resource-counts).
    void pktBeginLoad(AreaData *area, int argc, QStringList argv, AOPacket packet);

    /// Implements [character list](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#character-list).
    void pktRequestChars(AreaData *area, int argc, QStringList argv, AOPacket packet);

    /// Implements [music list](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#music-list).
    void pktRequestMusic(AreaData *area, int argc, QStringList argv, AOPacket packet);

    /// Implements [the final loading confirmation](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#final-confirmation).
    void pktLoadingDone(AreaData *area, int argc, QStringList argv, AOPacket packet);

    /**
     * @brief Implements character passwording. This is not on the netcode documentation as of writing.
     *
     * @todo Link packet details when it gets into the netcode documentation.
     */
    void pktCharPassword(AreaData *area, int argc, QStringList argv, AOPacket packet);

    /// Implements [character selection](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#choose-character).
    void pktSelectChar(AreaData *area, int argc, QStringList argv, AOPacket packet);

    /// Implements [the in-character messaging hell](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#in-character-message).
    void pktIcChat(AreaData *area, int argc, QStringList argv, AOPacket packet);

    /// Implements [out-of-character messages](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#out-of-character-message).
    void pktOocChat(AreaData *area, int argc, QStringList argv, AOPacket packet);

    /// Implements [the keepalive packet](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#keep-alive).
    void pktPing(AreaData *area, int argc, QStringList argv, AOPacket packet);

    /**
     * @brief Implements [music](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#music) and
     * [area changing](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#switch-area).
     */
    void pktChangeMusic(AreaData *area, int argc, QStringList argv, AOPacket packet);

    /**
     * @brief Implements [the witness testimony / cross examination / judge decision popups]
     * (https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#witness-testimonycross-examination-wtce).
     */
    void pktWtCe(AreaData *area, int argc, QStringList argv, AOPacket packet);

    /// Implements [penalty bars](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#penalty-health-bars).
    void pktHpBar(AreaData *area, int argc, QStringList argv, AOPacket packet);

    /**
     * @brief Implements WebSocket IP handling. This is not on the netcode documentation as of writing.
     *
     * @todo Link packet details when it gets into the netcode documentation.
     */
    void pktWebSocketIp(AreaData *area, int argc, QStringList argv, AOPacket packet);

    /// Implements [moderator calling](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#call-mod).
    void pktModCall(AreaData *area, int argc, QStringList argv, AOPacket packet);

    /// Implements [adding evidence](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#add).
    void pktAddEvidence(AreaData *area, int argc, QStringList argv, AOPacket packet);

    /// Implements [removing evidence](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#remove).
    void pktRemoveEvidence(AreaData *area, int argc, QStringList argv, AOPacket packet);

    /// Implements [editing evidence](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#edit).
    void pktEditEvidence(AreaData *area, int argc, QStringList argv, AOPacket packet);

    /// Implements [updating casing preferences](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#case-preferences-update).
    void pktSetCase(AreaData *area, int argc, QStringList argv, AOPacket packet);

    /// Implements [announcing a case](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#case-alert).
    void pktAnnounceCase(AreaData *area, int argc, QStringList argv, AOPacket packet);

    ///@}

    /**
     * @name Packet helper functions
     */
    ///@{

    /**
     * @brief Calls AOClient::updateEvidenceList() for every client in the current client's area.
     *
     * @param area The current client's area.
     */
    void sendEvidenceList(AreaData *area);

    /**
     * @brief Updates the evidence list in the area for the client.
     *
     * @param area The client's area.
     */
    void updateEvidenceList(AreaData *area);

    /**
     * @brief Attempts to validate that hellish abomination that Attorney Online 2 calls an in-character packet.
     *
     * @param packet The packet to validate.
     *
     * @return A validated version of the input packet if it is correct, or an `"INVALID"` packet if it is not.
     */
    AOPacket validateIcPacket(AOPacket packet);

    /**
     * @brief Removes excessive combining characters from a text.
     *
     * @param p_text The text to clear of its excessive combining characters.
     *
     * @return See brief description.
     *
     * @see https://en.wikipedia.org/wiki/Zalgo_text
     */
    QString dezalgo(QString p_text);

    /**
     * @brief Checks if the client can modify the evidence in the area.
     *
     * @param area The client's area.
     *
     * @return True if the client can modify the evidence, false if not.
     */
    bool checkEvidenceAccess(AreaData *area);

    ///@}

    /**
     * @name Packet helper global variables
     */
    ///@{

    /**
     * @brief The client's character ID.
     *
     * @details A character ID is just the character's index in the server's character list.
     *
     * In general, the client assumes that this is a continuous block starting from 0.
     */
    int m_char_id = -1;

    /**
     * @brief The character ID of the other character that the client wants to pair up with.
     *
     * @details Though this uses character ID, a client with *that* character ID must exist in the area for the pairing to work.
     * Furthermore, the owner of that character ID must also do the reverse to this client, making their `pairing_with` equal
     * to this client's character ID.
     */
    int m_pairing_with = -1;

    /**
     * @brief The name of the emote last used by the client. No extension.
     *
     * @details This is used for pairing mainly, for the server to be able to craft a smooth-looking transition from one
     * paired-up client talking to the next.
     */
    QString m_emote = "";

    /**
     * @brief The amount the client was last offset by.
     *
     * @details This used to be just a plain number ranging from -100 to 100, but then Crystal mangled it by building some extra data into it.
     * Cheers, love.
     */
    QString m_offset = "";

    /**
     * @brief The last flipped state of the client.
     */
    QString m_flipping = "";

    /**
     * @brief The last reported position of the client.
     */
    QString m_pos = "";

    ///@}

    /// Describes a packet's interpretation details.
    struct PacketInfo
    {
        unsigned long long acl_mask; //!< The permissions necessary for the packet.
        int minArgs;                 //!< The minimum arguments needed for the packet to be interpreted correctly / make sense.
        void (AOClient::*action)(AreaData *, int, QStringList, AOPacket);
    };

    /**
     * @property PacketInfo::action
     *
     * @brief A function reference that contains what the packet actually does.
     *
     * @param AreaData This is always just a reference to the data of the area the sender client is in. Used by some packets.
     * @param int When called, this parameter will be filled with the argument count.
     * @param QStringList When called, this parameter will be filled the list of arguments.
     * @param AOPacket This is a duplicated version of the QStringList above, containing the same data.
     */

    /**
     * @brief The list of packets that the server can interpret.
     *
     * @showinitializer
     *
     * @tparam QString The header of the packet that uniquely identifies it.
     * @tparam PacketInfo The details of the packet.
     * See @ref PacketInfo "the type's documentation" for more details.
     */
    const QMap<QString, PacketInfo> packets{
        {"HI", {ACLFlags.value("NONE"), 1, &AOClient::pktHardwareId}},
        {"ID", {ACLFlags.value("NONE"), 2, &AOClient::pktSoftwareId}},
        {"askchaa", {ACLFlags.value("NONE"), 0, &AOClient::pktBeginLoad}},
        {"RC", {ACLFlags.value("NONE"), 0, &AOClient::pktRequestChars}},
        {"RM", {ACLFlags.value("NONE"), 0, &AOClient::pktRequestMusic}},
        {"RD", {ACLFlags.value("NONE"), 0, &AOClient::pktLoadingDone}},
        {"PW", {ACLFlags.value("NONE"), 1, &AOClient::pktCharPassword}},
        {"CC", {ACLFlags.value("NONE"), 3, &AOClient::pktSelectChar}},
        {"MS", {ACLFlags.value("NONE"), 15, &AOClient::pktIcChat}},
        {"CT", {ACLFlags.value("NONE"), 2, &AOClient::pktOocChat}},
        {"CH", {ACLFlags.value("NONE"), 1, &AOClient::pktPing}},
        {"MC", {ACLFlags.value("NONE"), 2, &AOClient::pktChangeMusic}},
        {"RT", {ACLFlags.value("NONE"), 1, &AOClient::pktWtCe}},
        {"HP", {ACLFlags.value("NONE"), 2, &AOClient::pktHpBar}},
        {"WSIP", {ACLFlags.value("NONE"), 1, &AOClient::pktWebSocketIp}},
        {"ZZ", {ACLFlags.value("NONE"), 0, &AOClient::pktModCall}},
        {"PE", {ACLFlags.value("NONE"), 3, &AOClient::pktAddEvidence}},
        {"DE", {ACLFlags.value("NONE"), 1, &AOClient::pktRemoveEvidence}},
        {"EE", {ACLFlags.value("NONE"), 4, &AOClient::pktEditEvidence}},
        {"SETCASE", {ACLFlags.value("NONE"), 7, &AOClient::pktSetCase}},
        {"CASEA", {ACLFlags.value("NONE"), 6, &AOClient::pktAnnounceCase}},
    };

    /**
     * @name Authentication
     */
    ///@{

    /**
     * @brief Sets the client to be in the process of logging in, setting is_logging_in to **true**.
     *
     * @details No arguments.
     *
     * @iscommand
     */
    void cmdLogin(int argc, QStringList argv);

    /**
     * @brief Starts the authorisation type change from `"simple"` to `"advanced"`.
     *
     * @details No arguments.
     *
     * @iscommand
     */
    void cmdChangeAuth(int argc, QStringList argv);

    /**
     * @brief Sets the root user's password.
     *
     * @details Accepts a single argument that will be the **root user's password**.
     *
     * @iscommand
     *
     * @pre AOClient::cmdChangeAuth()
     */
    void cmdSetRootPass(int argc, QStringList argv);

    /**
     * @brief Adds a user to the moderators in `"advanced"` authorisation type.
     *
     * @details The first argument is the **user's name**, the second is their **password**.
     *
     * @iscommand
     */
    void cmdAddUser(int argc, QStringList argv);

    /**
     * @brief Removes a user from the moderators in `"advanced"` authorisation type.
     *
     * @details Takes the **targer user's name** as the argument.
     *
     * @iscommand
     */
    void cmdRemoveUser(int argc, QStringList argv);

    /**
     * @brief Lists the permission of a given user.
     *
     * @details If called without argument, lists the caller's permissions.
     *
     * If called with one argument, **a username**, lists that user's permissions.
     *
     * @iscommand
     */
    void cmdListPerms(int argc, QStringList argv);

    /**
     * @brief Adds permissions to a given user.
     *
     * @details The first argument is the **target user**, the second is the **permission** (in string form) to add to that user.
     *
     * @iscommand
     */
    void cmdAddPerms(int argc, QStringList argv);

    /**
     * @brief Removes permissions from a given user.
     *
     * @details The first argument is the **target user**, the second is the **permission** (in string form) to remove from that user.
     *
     * @iscommand
     */
    void cmdRemovePerms(int argc, QStringList argv);

    /**
     * @brief Lists all users in the server's database.
     *
     * @details No arguments.
     *
     * @iscommand
     */
    void cmdListUsers(int argc, QStringList argv);

    /**
     * @brief Logs the caller out from their moderator user.
     *
     * @details No arguments.
     *
     * @iscommand
     */
    void cmdLogout(int argc, QStringList argv);

    /**
     * @brief Changes a moderator's password.
     *
     * @details If it is called with **one argument**, that argument is the **new password** to change to.
     *
     * If it is called with **two arguments**, the first argument is the **new password** to change to,
     * and the second argument is the **username** of the moderator to change the password of.
     */
    void cmdChangePassword(int argc, QStringList argv);

    ///@}

    /**
     * @name Areas
     *
     * @brief All functions that detail the actions of commands,
     * that are also related to area management.
     */
    ///@{

    /**
     * @brief Promotes a client to CM status.
     *
     * @details If called without arguments, promotes the caller.
     *
     * If called with a **user ID** as an argument, and the caller is a CM, promotes the target client to CM status.
     *
     * @iscommand
     */
    void cmdCM(int argc, QStringList argv);

    /**
     * @brief Removes the CM status from the caller.
     *
     * @details No arguments.
     *
     * @iscommand
     */
    void cmdUnCM(int argc, QStringList argv);

    /**
     * @brief Invites a client to the area.
     *
     * @details Needs a **user ID** as an argument.
     *
     * @iscommand
     *
     * @see AreaData::LOCKED and AreaData::SPECTATABLE for the benefits of being invited.
     */
    void cmdInvite(int argc, QStringList argv);

    /**
     * @brief Uninvites a client to the area.
     *
     * @details Needs a **user ID** as an argument.
     *
     * @iscommand
     *
     * @see AreaData::LOCKED and AreaData::SPECTATABLE for the benefits of being invited.
     */
    void cmdUnInvite(int argc, QStringList argv);

    /**
     * @brief Locks the area.
     *
     * @details No arguments.
     *
     * @iscommand
     *
     * @see AreaData::LOCKED
     */
    void cmdLock(int argc, QStringList argv);

    /**
     * @brief Sets the area to spectatable.
     *
     * @details No arguments.
     *
     * @iscommand
     *
     * @see AreaData::SPECTATABLE
     */
    void cmdSpectatable(int argc, QStringList argv);

    /**
     * @brief Unlocks the area.
     *
     * @details No arguments.
     *
     * @iscommand
     *
     * @see AreaData::FREE
     */
    void cmdUnLock(int argc, QStringList argv);

    /**
     * @brief Lists all clients in all areas.
     *
     * @details No arguments.
     *
     * @iscommand
     */
    void cmdGetAreas(int argc, QStringList argv);

    /**
     * @brief Lists all clients in the area the caller is in.
     *
     * @details No arguments.
     *
     * @iscommand
     */
    void cmdGetArea(int argc, QStringList argv);

    /**
     * @brief Moves the caller to the area with the given ID.
     *
     * @details Takes an **area ID** as an argument.
     *
     * @iscommand
     */
    void cmdArea(int argc, QStringList argv);

    /**
     * @brief Kicks a client from the area, moving them back to the default area.
     *
     * @details Takes one argument, the **client's ID** to kick.
     *
     * @iscommand
     */
    void cmdAreaKick(int argc, QStringList argv);

    /**
     * @brief Changes the background of the current area.
     *
     * @details Takes the **background's internal name** (generally the background's directory's name for the clients) as the only argument.
     *
     * @iscommand
     */
    void cmdSetBackground(int argc, QStringList argv);

    /**
     * @brief Locks the background, preventing it from being changed.
     *
     * @details No arguments.
     *
     * @iscommand
     */
    void cmdBgLock(int argc, QStringList argv);

    /**
     * @brief Unlocks the background, allowing it to be changed again.
     *
     * @details No arguments.
     *
     * @iscommand
     */
    void cmdBgUnlock(int argc, QStringList argv);

    /**
     * @brief Changes the status of the current area.
     *
     * @details Takes a **status** as an argument. See AreaData::Status for permitted values.
     *
     * @iscommand
     */
    void cmdStatus(int argc, QStringList argv);

    /**
     * @brief Sends an out-of-character message with the judgelog of an area.
     *
     * @details No arguments.
     *
     * @iscommand
     */
    void cmdJudgeLog(int argc, QStringList argv);

    /**
     * @brief Toggles whether the BG list is ignored in an area.
     *
     * @details No arguments.
     *
     * @iscommand
     */
    void cmdIgnoreBgList(int argc, QStringList argv);

    /**
     * @brief Returns the area message in OOC. Double to set the current area message.
     *
     * @details See short description.
     *
     * @iscommand
     */
    void cmdAreaMessage(int argc, QStringList argv);

    /**
     * @brief Clears the areas message and disables automatic sending.
     *
     * @details No arguments.
     *
     * @iscommand
     */
    void cmdClearAreaMessage(int argc, QStringList argv);

    /**
     * @brief Toggles wether the client shows the area message when joining the current area.
     *
     * @details No arguments.
     *
     * @iscommand
     */
    void cmdToggleAreaMessageOnJoin(int argc, QStringList argv);

    ///@}

    /**
     * @name Moderation
     *
     * @brief All functions that detail the actions of commands,
     * that are also related to the moderation and administration of the server.
     */
    ///@{

    /**
     * @brief Lists all the commands that the caller client has the permissions to use.
     *
     * @details No arguments.
     *
     * @iscommand
     */
    void cmdCommands(int argc, QStringList argv);

    /**
     * @brief Lists help information to the command requested. Includes syntax and brief explanation.
     *
     * @details Takes the command name as an argument.
     */
    void cmdHelp(int argc, QStringList argv);

    /**
     * @brief Gets or sets the server's Message Of The Day.
     *
     * @details If called without arguments, gets the MOTD.
     *
     * If it has any number of arguments, it is set as the **MOTD**.
     *
     * @iscommand
     */
    void cmdMOTD(int argc, QStringList argv);

    /**
     * @brief Gives a very brief description of Akashi.
     *
     * @details No arguments.
     *
     * @iscommand
     */
    void cmdAbout(int argc, QStringList argv);

    /**
     * @brief Lists the currently logged-in moderators on the server.
     *
     * @details No arguments.
     *
     * @iscommand
     */
    void cmdMods(int argc, QStringList argv);

    /**
     * @brief Bans a client from the server, forcibly severing its connection to the server,
     * and disallowing their return.
     *
     * @details The first argument is the **target's IPID**, the second is the **duration**,
     * and the third is the **reason** why the client was banned.
     *
     * The duration can be `perma`, meaning a forever ban, otherwise, it must be given in the format of `YYyWWwDDdHHhMMmSSs` to
     * mean a YY years, WW weeks, DD days, HH hours, MM minutes and SS seconds long ban. Any of these may be left out, for example,
     * `1h30m` for a 1.5 hour long ban.
     *
     * Besides banning, this command kicks all clients having the given IPID,
     * thus a multiclienting user will have all their clients be kicked from the server.
     *
     * The target's hardware ID is also recorded in a ban, so users with dynamic IPs will not be able to
     * cirvumvent the ban without also changing their hardware ID.
     *
     * @iscommand
     */
    void cmdBan(int argc, QStringList argv);

    /**
     * @brief Removes a ban from the database.
     *
     * @details Takes a single argument, the **ID** of the ban.
     *
     * @iscommand
     */
    void cmdUnBan(int argc, QStringList argv);

    /**
     * @brief Kicks a client from the server, forcibly severing its connection to the server.
     *
     * @details The first argument is the **target's IPID**, while the remaining arguments are the **reason**
     * the client was kicked. Both arguments are mandatory.
     *
     * This command kicks all clients having the given IPID, thus a multiclienting user will have all
     * their clients be kicked from the server.
     *
     * @iscommand
     */
    void cmdKick(int argc, QStringList argv);

    /**
     * @brief Mutes a client.
     *
     * @details The only argument is the **target client's user ID**.
     *
     * @iscommand
     *
     * @see #is_muted
     */
    void cmdMute(int argc, QStringList argv);

    /**
     * @brief Removes the muted status from a client.
     *
     * @details The only argument is the **target client's user ID**.
     *
     * @iscommand
     *
     * @see #is_muted
     */
    void cmdUnMute(int argc, QStringList argv);

    /**
     * @brief OOC-mutes a client.
     *
     * @details The only argument is the **target client's user ID**.
     *
     * @iscommand
     *
     * @see #is_ooc_muted
     */
    void cmdOocMute(int argc, QStringList argv);

    /**
     * @brief Removes the OOC-muted status from a client.
     *
     * @details The only argument is the **target client's user ID**.
     *
     * @iscommand
     *
     * @see #is_ooc_muted
     */
    void cmdOocUnMute(int argc, QStringList argv);

    /**
     * @brief WTCE-blocks a client.
     *
     * @details The only argument is the **target client's user ID**.
     *
     * @iscommand
     *
     * @see #is_wtce_blocked
     */
    void cmdBlockWtce(int argc, QStringList argv);

    /**
     * @brief Removes the WTCE-blocked status from a client.
     *
     * @details The only argument is the **target client's user ID**.
     *
     * @iscommand
     *
     * @see #is_wtce_blocked
     */
    void cmdUnBlockWtce(int argc, QStringList argv);

    /**
     * @brief Lists the last five bans made on the server.
     *
     * @details No arguments.
     *
     * @iscommand
     */
    void cmdBans(int argc, QStringList argv);

    /**
     * @brief Toggle whether or not in-character messages purely consisting of spaces are allowed.
     *
     * @details Takes no arguments. Against all common sense this also allows you to disable blankposting.
     *
     * @iscommand
     */
    void cmdAllowBlankposting(int argc, QStringList argv);

    /**
     * @brief Looks up info on a ban.
     *
     * @details If it is called with **one argument**, that argument is the ban ID to look up.
     *
     * If it is called with **two arguments**, then the first argument is either a ban ID, an IPID,
     * or an HDID, and the the second argument specifies the ID type.
     *
     * @iscommand
     */
    void cmdBanInfo(int argc, QStringList argv);

    /**
     * @brief Reloads all server configuration files.
     *
     * @details No arguments.
     *
     * @iscommand
     */
    void cmdReload(int argc, QStringList argv);

    /**
     * @brief Toggles immediate text processing in the current area.
     *
     * @details No arguments.
     *
     * @iscommand
     */
    void cmdForceImmediate(int argc, QStringList argv);

    /**
     * @brief Toggles whether iniswaps are allowed in the current area.
     *
     * @details No arguments.
     *
     * @iscommand
     */
    void cmdAllowIniswap(int argc, QStringList argv);

    /**
     * @brief Grants a client the temporary permission to save a testimony.
     *
     * @details ClientID as the target of permission
     *
     * @iscommand
     *
     */
    void cmdPermitSaving(int argc, QStringList argv);

    /**
     * @brief Kicks a client from the server, forcibly severing its connection to the server.
     *
     * @details The first argument is the **target's UID**, while the remaining arguments are the **reason**
     * the client was kicked. Both arguments are mandatory.
     *
     * Unlike cmdKick, this command will only kick a single client, thus a multiclienting user will not have all their clients kicked.
     *
     * @iscommand
     *
     * @see #cmdKick
     */
    void cmdKickUid(int argc, QStringList argv);

    /**
     * @brief Updates a ban in the database, changing either its reason or duration.
     *
     * @details The first argument is the **ID** of the ban to update. The second argument is the **field** to update, either `reason` or `duration`
     *
     * and the remaining arguments are the **duration** or the **reason** to update to.
     *
     * @iscommand
     */
    void cmdUpdateBan(int argc, QStringList argv);

    /**
     * @brief Pops up a notice for all clients in the targeted area with a given message.
     *
     * @details The only argument is the message to send. This command only works on clients with support for the BB#%
     *
     * generic message box packet (i.e. Attorney Online versions 2.9 and up). To support earlier versions (as well as to make the message
     *
     * re-readable if a user clicks out of it too fast) the message will also be sent in OOC to all affected clients.
     *
     * @iscommand
     */
    void cmdNotice(int argc, QStringList argv);

    /**
     * @brief Pops up a notice for all clients in the server with a given message.
     *
     * @details Unlike cmdNotice, this command will send its notice to every client connected to the server.
     *
     * @see #cmdNotice
     *
     * @iscommand
     */
    void cmdNoticeGlobal(int argc, QStringList argv);

    /**
     * @brief Removes all CMs from the current area.
     *
     * @details This command is a bandaid fix to the issue that clients may end up ghosting when improperly disconnected from the server.
     *
     * @iscommand
     */
    void cmdClearCM(int argc, QStringList argv);

    ///@}

    /**
     * @name Roleplay
     *
     * @brief All functions that detail the actions of commands,
     * that are also related to various kinds of roleplay actions in some way.
     */
    ///@{

    /**
     * @brief Flips a coin, returning heads or tails.
     *
     * @details No arguments.
     *
     * @iscommand
     */
    void cmdFlip(int argc, QStringList argv);

    /**
     * @brief Rolls dice and sends the results.
     *
     * @details The first argument is the **amount of faces** each die should have.
     * The second argument is the **amount of dice** that should be rolled.
     *
     * Both arguments are optional.
     *
     * @iscommand
     *
     * @see AOClient::diceThrower
     */
    void cmdRoll(int argc, QStringList argv);

    /**
     * @brief Rolls dice, but sends the results in private to the roller.
     *
     * @copydetails AOClient::cmdRoll
     */
    void cmdRollP(int argc, QStringList argv);

    /**
     * @brief Gets or sets the global or one of the area-specific timers.
     *
     * @details If called without arguments, sends an out-of-character message listing the statuses of both
     * the global timer and the area-specific timers.
     *
     * If called with **one argument**, and that argument is between `0` and `4` (inclusive on both ends),
     * sends an out-of-character message about the status of the given timer, where `0` is the global timer,
     * and the remaining numbers are the first, second, third and fourth timers in the current area.
     *
     * If called with **two arguments**, and the second argument is
     * * in the format of `hh:mm:ss`, then it starts the given timer,
     *   with `hh` hours, `mm` minutes, and `ss` seconds on it, making it appear if needed.
     * * `start`, it (re)starts the given timer, making it appear if needed.
     * * `pause` or `stop`, it pauses the given timer.
     * * `hide` or `unset`, it stops the timer and hides it.
     *
     * @iscommand
     */
    void cmdTimer(int argc, QStringList argv);

    /**
     * @brief Changes the subtheme of the clients in the current area.
     *
     * @details The only argument is the **name of the subtheme**. Reloading is always forced.
     *
     * @iscommand
     */

    void cmdSubTheme(int argc, QStringList argv);

    /**
     * @brief Writes a "note card" in the current area.
     *
     * @details The note card is not readable until all note cards in the area are revealed by a **CM**.
     * A message will appear to all clients in the area indicating that a note card has been written.
     *
     * @iscommand
     */
    void cmdNoteCard(int argc, QStringList argv);

    /**
     * @brief Reveals all note cards in the current area.
     *
     * @iscommand
     */
    void cmdNoteCardReveal(int argc, QStringList argv);

    /**
     * @brief Erases the client's note card from the area's list of cards.
     *
     * @details A message will appear to all clients in the area indicating that a note card has been erased.
     *
     * @iscommand
     */
    void cmdNoteCardClear(int argc, QStringList argv);

    /**
     * @brief Randomly selects an answer from 8ball.txt to a question.
     *
     * @details The only argument is the question the client wants answered.
     *
     * @iscommand
     */
    void cmd8Ball(int argc, QStringList argv);

    ///@}

    /**
     * @name Messaging
     *
     * @brief All functions that detail the actions of commands,
     * that are also related to messages or the client's self-management in some way.
     */
    ///@{

    /**
     * @brief Changes the client's position.
     *
     * @details The only argument is the **target position** to move the client to.
     *
     * @iscommand
     */
    void cmdPos(int argc, QStringList argv);

    /**
     * @brief Forces a client, or all clients in the area, to a specific position.
     *
     * @details The first argument is the **client's ID**, or `\*` if the client wants to force all
     * clients in their area into the position.
     * If a specific client ID is given, this command can reach across areas, i.e., the caller and target
     * clients don't have to share areas.
     *
     * The second argument is the **position** to force the clients to.
     * This is not checked for nonsense values.
     *
     * @iscommand
     */
    void cmdForcePos(int argc, QStringList argv);

    /**
     * @brief Switches to a different character based on character ID.
     *
     * @details The only argument is the **character's ID** that the client wants to switch to.
     *
     * @iscommand
     */
    void cmdSwitch(int argc, QStringList argv);

    /**
     * @brief Picks a new random character for the client.
     *
     * @details No arguments.
     *
     * @iscommand
     */
    void cmdRandomChar(int argc, QStringList argv);

    /**
     * @brief Sends a global message (i.e., all clients in the server will be able to see it).
     *
     * @details The arguments are **the message** that the client wants to send.
     *
     * @iscommand
     */
    void cmdG(int argc, QStringList argv);

    /**
     * @brief Toggles whether the client will ignore @ref cmdG "global" messages or not.
     *
     * @details No arguments.
     *
     * @iscommand
     */
    void cmdToggleGlobal(int argc, QStringList argv);

    /**
     * @brief Sends a direct message to another client on the server based on ID.
     *
     * @details Has two arguments, the first is **the target's ID** whom the client wants to send the message to,
     * while the second is **the message** the client wants to send.
     *
     * The "second" part is technically everything that isn't the first argument.
     *
     * @iscommand
     */
    void cmdPM(int argc, QStringList argv);

    /**
     * @brief A global message expressing that the client needs something (generally: players for something).
     *
     * @details The arguments are **the message** that the client wants to send.
     *
     * @iscommand
     */
    void cmdNeed(int argc, QStringList argv);

    /**
     * @brief Sends out a decorated global message, for announcements.
     *
     * @details The arguments are **the message** that the client wants to send.
     *
     * @iscommand
     *
     * @see AOClient::cmdG()
     */
    void cmdAnnounce(int argc, QStringList argv);

    /**
     * @brief Sends a message in the server-wide, moderator only chat.
     *
     * @details The arguments are **the message** that the client wants to send.
     *
     * @iscommand
     */
    void cmdM(int argc, QStringList argv);

    /**
     * @brief Sends out a global message that is marked with an `[M]` to mean it is coming from a moderator.
     *
     * @details The arguments are **the message** that the client wants to send.
     *
     * @iscommand
     *
     * @see AOClient::cmdG()
     */
    void cmdGM(int argc, QStringList argv);

    /**
     * @brief Sends out a local message that is marked with an `[M]` to mean it is coming from a moderator.
     *
     * @details The arguments are **the message** that the client wants to send.
     *
     * @iscommand
     *
     * @see AOClient::cmdLM()
     */
    void cmdLM(int argc, QStringList argv);

    /**
     * @brief Replaces a target client's in-character messages with strings randomly selected from gimp.txt.
     *
     * @details The only argument is the **the target's ID** whom the client wants to gimp.
     *
     * @iscommand
     */
    void cmdGimp(int argc, QStringList argv);

    /**
     * @brief Allows a gimped client to speak normally.
     *
     * @details The only argument is **the target's ID** whom the client wants to ungimp.
     *
     * @iscommand
     */
    void cmdUnGimp(int argc, QStringList argv);

    /**
     * @brief Removes all vowels from a target client's in-character messages.
     *
     * @details The only argument is **the target's ID** whom the client wants to disemvowel.
     *
     * @iscommand
     */
    void cmdDisemvowel(int argc, QStringList argv);

    /**
     * @brief Allows a disemvoweled client to speak normally.
     *
     * @details The only argument is **the target's ID** whom the client wants to undisemvowel.
     *
     * @iscommand
     */
    void cmdUnDisemvowel(int argc, QStringList argv);

    /**
     * @brief Scrambles the words of a target client's in-character messages.
     *
     * @details The only argument is **the target's ID** whom the client wants to shake.
     *
     * @iscommand
     */
    void cmdShake(int argc, QStringList argv);

    /**
     * @brief Allows a shaken client to speak normally.
     *
     * @details The only argument is **the target's ID** whom the client wants to unshake.
     *
     * @iscommand
     */
    void cmdUnShake(int argc, QStringList argv);

    /**
     * @brief Toggles whether a client will recieve @ref cmdPM private messages or not.
     *
     * @details No arguments.
     *
     * @iscommand
     */
    void cmdMutePM(int argc, QStringList argv);

    /**
     * @brief Toggles whether a client will recieve @ref cmdNeed "advertisement" messages.
     *
     * @details No arguments.
     *
     * @iscommand
     */
    void cmdToggleAdverts(int argc, QStringList argv);

    /**
     * @brief Toggles whether this client is considered AFK.
     *
     * @details No arguments.
     *
     * @iscommand
     */
    void cmdAfk(int argc, QStringList argv);

    /**
     * @brief Restricts a target client to a set of characters that they can switch from, blocking them from other characters.
     *
     * @details The first argument is the **target's ID** whom the client wants to charcurse.
     *
     * The second argument is one or more character names the client wants to restrict to, comma separated.
     *
     * @iscommand
     */
    void cmdCharCurse(int argc, QStringList argv);

    /**
     * @brief Removes the charcurse status from a client.
     *
     * @details The only argument is the **target's ID** whom the client wants to uncharcurse.
     *
     * @iscommand
     */
    void cmdUnCharCurse(int argc, QStringList argv);

    /**
     * @brief Forces a client into the charselect screen.
     *
     * @details The only argument is the **target's ID** whom the client wants to force into charselect.
     *
     * @iscommand
     */
    void cmdCharSelect(int argc, QStringList argv);

    /**
     * @brief Sends a message to an area that you a CM in.
     *
     * @details Usage: /a <area> <message>
     *
     * @iscommand
     */
    void cmdA(int argc, QStringList argv);

    /**
     * @brief Send a message to all areas that you are a CM in.
     *
     * @details Usage: /s <message>
     *
     * @iscommand
     */
    void cmdS(int argc, QStringList argv);

    /**
     * @brief Toggle whether the client's messages will be sent in first person mode.
     *
     * @details No arguments.
     *
     * @iscommand
     */
    void cmdFirstPerson(int argc, QStringList argv);

    ///@}

    /**
     * @name Casing
     *
     * @brief All functions that detail the actions of commands,
     * that are related to casing.
     */
    ///@{

    /**
     * @brief Sets the `/doc` to a custom text.
     *
     * @details The arguments are **the text** that the client wants to set the doc to.
     *
     * @iscommand
     */
    void cmdDoc(int argc, QStringList argv);

    /**
     * @brief Sets the `/doc` to `"No document."`.
     *
     * @details No arguments.
     *
     * @iscommand
     */
    void cmdClearDoc(int argc, QStringList argv);

    /**
     * @brief Changes the evidence mod in the area.
     *
     * @details The only argument is the **evidence mod** to change to.
     *
     * @iscommand
     *
     * @see AreaData::EvidenceMod
     */
    void cmdEvidenceMod(int argc, QStringList argv);

    /**
     * @brief Changes position of two pieces of evidence in the area.
     *
     * @details The two arguments are the indices of the evidence items you want to swap the position of.
     *
     * @iscommand
     *
     * @see Area::Evidence_Swap
     *
     */
    void cmdEvidence_Swap(int argc, QStringList argv);

    /**
     * @brief Sets are to PLAYBACK mode
     *
     * @details Enables control over the stored testimony, prevent new messages to be added and
     * allows people to navigate trough it using > and <.
     */
    void cmdExamine(int argc, QStringList argv);

    /**
     * @brief Enables the testimony recording functionality.
     *
     * @details Any IC-Message send after this command is issues will be recorded by the testimony recorder.
     */
    void cmdTestify(int argc, QStringList argv);

    /**
     * @brief Allows user to update the currently displayed IC-Message from the testimony replay.
     *
     * @details Using this command replaces the content of the current statement entirely. It does not append information.
     */
    void cmdUpdateStatement(int argc, QStringList argv);

    /**
     * @brief Deletes a statement from the testimony.
     *
     * @details Using this deletes the entire entry in the QVector and resizes it appropriately to prevent empty record indices.
     */
    void cmdDeleteStatement(int argc, QStringList argv);

    /**
     * @brief Pauses testimony playback.
     *
     * @details Disables the testimony playback controls.
     */
    void cmdPauseTestimony(int argc, QStringList argv);

    /**
     * @brief Adds a statement to an existing testimony.
     *
     * @details Inserts new statement after the currently displayed recorded message. Increases the index by 1.
     *
     */
    void cmdAddStatement(int argc, QStringList argv);

    /**
     * @brief Sends a list of the testimony to OOC of the requesting client
     *
     * @details Retrieves all stored IC-Messages of the area and dumps them into OOC with some formatting.
     *
     */
    void cmdTestimony(int argc, QStringList argv);

    /**
     * @brief Saves a testimony recording to the servers storage.
     *
     * @details Saves a titled text file which contains the edited packets into a text file.
     *          The filename will always be lowercase.
     *
     */
    void cmdSaveTestimony(int argc, QStringList argv);

    /**
     * @brief Loads testimony for the testimony replay. Argument is the testimony name.
     *
     * @details Loads a titled text file which contains the edited packets to be loaded into the QVector.
     *          Validates the size of the testimony to ensure the entire testimony can be replayed.
     *          Testimony name will always be converted to lowercase.
     *
     */
    void cmdLoadTestimony(int argc, QStringList argv);

    ///@}

    /**
     * @name Music
     *
     * @brief All functions that detail the actions of commands,
     * that are also related to music in some way.
     */
    ///@{

    /**
     * @brief Plays music in the area.
     *
     * @details The arguments are **the song's filepath** originating from `base/sounds/music/`,
     * or **the song's URL** if it's a stream.
     *
     * As described above, this command can be used to play songs by URL (for clients at and above version 2.9),
     * but it can also be used to play songs locally available for the clients but not listed in the music list.
     *
     * @iscommand
     */
    void cmdPlay(int argc, QStringList argv);

    /**
     * @brief DJ-blocks a client.
     *
     * @details The only argument is the **target client's user ID**.
     *
     * @iscommand
     *
     * @see #is_dj_blocked
     */
    void cmdBlockDj(int argc, QStringList argv);

    /**
     * @brief Removes the DJ-blocked status from a client.
     *
     * @details The only argument is the **target client's user ID**.
     *
     * @iscommand
     *
     * @see #is_dj_blocked
     */
    void cmdUnBlockDj(int argc, QStringList argv);

    /**
     * @brief Returns the currently playing music in the area, and who played it.
     *
     * @details No arguments.
     *
     * @iscommand
     */
    void cmdCurrentMusic(int argc, QStringList argv);

    /**
     * @brief Toggles music playing in the current area.
     *
     * @details No arguments.
     */
    void cmdToggleMusic(int argc, QStringList argv);

    /**
     * @brief Toggles jukebox status in the current area.
     *
     * @details No arguments.
     */
    void cmdToggleJukebox(int argc, QStringList argv);

    /**
     * @brief Adds a song to the custom list.
     */
    void cmdAddSong(int argc, QStringList argv);

    /**
     * @brief Adds a category to the areas custom music list.
     */
    void cmdAddCategory(int argc, QStringList argv);

    /**
     * @brief Removes any matching song or category from the custom area.
     */
    void cmdRemoveCategorySong(int argc, QStringList argv);

    /**
     * @brief Toggles the prepending behaviour of the servers root musiclist.
     */
    void cmdToggleRootlist(int argc, QStringList argv);

    /**
     * @brief Clears the entire custom list of this area.
     */
    void cmdClearCustom(int argc, QStringList argv);

    ///@}

    /**
     * @name Command helper functions
     *
     * @brief A collection of functions of shared behaviour between command functions,
     * allowing the abstraction of technical details in the command function definition,
     * or the avoidance of repetition over multiple definitions.
     */
    ///@{

    /**
     * @brief Literally just an invalid default command. That's it.
     *
     * @note Can be used as a base for future commands.
     *
     * @iscommand
     */
    void cmdDefault(int argc, QStringList argv);

    /**
     * @brief Returns a textual representation of the time left in an area's Timer.
     *
     * @param area_idx The ID of the area whose timer to grab.
     * @param timer_idx The ID of the timer to grab
     *
     * @return A textual representation of the time left over on the Timer,
     * or `"Timer is inactive"` if the timer wasn't started.
     */
    QString getAreaTimer(int area_idx, int timer_idx);

    /**
     * @brief Generates a tsuserver3-style area list to be displayed to the user in the out-of-character chat.
     *
     * @details This list shows general details about the area the caller is currently in,
     * like who is the owner, what players are in there, the status of the area, etc.
     *
     * @param area_idx The index of the area whose details should be generated.
     *
     * @return A QStringList of details about the given area, with every entry in the string list amounting to
     * roughly a separate line.
     */
    QStringList buildAreaList(int area_idx);

    /**
     * @brief Convenience function to generate a random integer number between the given minimum and maximum values.
     *
     * @param min The minimum possible value for the random integer, inclusive.
     * @param max The maximum possible value for the random integer, exclusive.
     *
     * @warning `max` must be greater than `min`.
     *
     * @return A randomly generated integer within the bounds given.
     */
    int genRand(int min, int max);

    /**
     * @brief A convenience function for rolling dice.
     *
     * @param argc The amount of arguments.
     *
     * @param argv Stringlist of the arguments given by the client.
     *
     * @param p_roll Bool to determine of a roll is private or not.
     */
    void diceThrower(int argc, QStringList argv, bool p_roll);

    /**
     * @brief Interprets an expression of time into amount of seconds.
     *
     * @param input A string in the format of `"XXyXXwXXdXXhXXmXXs"`, where every `XX` is some integer.
     * There is no limit on the length of the integers, the `XX` text is just a placeholder, and is not intended to
     * indicate a limit of two digits maximum.
     *
     * The string gets interpreted as follows:
     * * `XXy` is parsed into `XX` amount of years,
     * * `XXw` is parsed into `XX` amount of weeks,
     * * `XXd` is parsed into `XX` amount of days,
     * * `XXh` is parsed into `XX` amount of hours,
     * * `XXm` is parsed into `XX` amount of minutes, and
     * * `XXs` is parsed into `XX` amount of seconds.
     *
     * Any of these may be left out, but the order must be kept (i.e., `"10s5y"` is a malformed text).
     *
     * @return The parsed text, converted into their respective durations, summed up, then converted into seconds.
     */
    long long parseTime(QString input);
    QString getReprimand(bool f_positive = false);

    /**
     * @brief Adds the last send IC-Message to QVector of the respective area.
     *
     * @details This one pulls double duty to both append IC-Messages to the QVector or insert them, depending on the current recorder enum.
     *
     * @param packet The MS-Packet being recorded with their color changed to green.
     */
    void addStatement(QStringList packet);

    /**
     * @brief Clears QVector of the current area.
     *
     * @details It clears both its content and trims it back to size 0
     *
     */
    void clearTestimony();

    /**
     * @brief Updates the currently displayed IC-Message with the next one send
     * @param packet The IC-Message that will overwrite the currently stored one.
     * @return Returns the updated IC-Message to be send to the other users. It also changes the color to green.
     */
    QStringList updateStatement(QStringList packet);

    /**
     * @brief Called when area enum is set to PLAYBACK. Sends the IC-Message stored at the current statement.
     * @return IC-Message stored in the QVector.
     */
    QStringList playTestimony();

    /**
     * @brief Checks if a password meets the server's password requirements.
     *
     * @param username The chosen username.
     *
     * @param password The password to check.
     *
     * @return True if the password meets the requirements, otherwise false.
     */
    bool checkPasswordRequirements(QString f_username, QString f_password);

    /**
     * @brief Sends a server notice.
     *
     * @param notice The notice to send out.
     *
     * @param global Whether or not the notice should be server-wide.
     */
    void sendNotice(QString f_notice, bool f_global = false);

    /**
     * @brief Checks if a testimony contains '<' or '>'.
     *
     * @param message The IC Message that might contain unproper symbols.
     *
     * @return True if it contains '<' or '>' symbols, otherwise false.
     */
    bool checkTestimonySymbols(const QString &message);
    ///@}

    /**
     * @brief A helper variable that is used to direct the called of the `/changeAuth` command through the process
     * of changing the authorisation method from simple to advanced.
     *
     * @see cmdChangeAuth and cmdSetRootPass
     */
    bool change_auth_started = false;

    /**
     * @brief Describes a command's details.
     */
    struct CommandInfo
    {
        unsigned long long acl_mask; //!< The permissions necessary to be able to run the command. @see ACLFlags.
        int minArgs;                 //!< The minimum mandatory arguments needed for the command to function.
        void (AOClient::*action)(int, QStringList);
    };

    /**
     * @property CommandInfo::action
     *
     * @brief A function reference that contains what the command actually does.
     *
     * @param int When called, this parameter will be filled with the argument count. @anchor commandArgc
     * @param QStringList When called, this parameter will be filled the list of arguments. @anchor commandArgv
     */

    /**
     * @brief The list of commands available on the server.
     *
     * @details Generally called with the format of `/command parameters` in the out-of-character chat.
     * @showinitializer
     *
     * @tparam QString The name of the command, without the leading slash.
     * @tparam CommandInfo The details of the command.
     * See @ref CommandInfo "the type's documentation" for more details.
     */
    const QMap<QString, CommandInfo> commands{
        {"login", {ACLFlags.value("NONE"), 0, &AOClient::cmdLogin}},
        {"getareas", {ACLFlags.value("NONE"), 0, &AOClient::cmdGetAreas}},
        {"gas", {ACLFlags.value("NONE"), 0, &AOClient::cmdGetAreas}},
        {"getarea", {ACLFlags.value("NONE"), 0, &AOClient::cmdGetArea}},
        {"ga", {ACLFlags.value("NONE"), 0, &AOClient::cmdGetArea}},
        {"ban", {ACLFlags.value("BAN"), 3, &AOClient::cmdBan}},
        {"kick", {ACLFlags.value("KICK"), 2, &AOClient::cmdKick}},
        {"changeauth", {ACLFlags.value("SUPER"), 0, &AOClient::cmdChangeAuth}},
        {"rootpass", {ACLFlags.value("SUPER"), 1, &AOClient::cmdSetRootPass}},
        {"background", {ACLFlags.value("NONE"), 1, &AOClient::cmdSetBackground}},
        {"bg", {ACLFlags.value("NONE"), 1, &AOClient::cmdSetBackground}},
        {"bglock", {ACLFlags.value("BGLOCK"), 0, &AOClient::cmdBgLock}},
        {"bgunlock", {ACLFlags.value("BGLOCK"), 0, &AOClient::cmdBgUnlock}},
        {"adduser", {ACLFlags.value("MODIFY_USERS"), 2, &AOClient::cmdAddUser}},
        {"listperms", {ACLFlags.value("NONE"), 0, &AOClient::cmdListPerms}},
        {"addperm", {ACLFlags.value("MODIFY_USERS"), 2, &AOClient::cmdAddPerms}},
        {"removeperm", {ACLFlags.value("MODIFY_USERS"), 2, &AOClient::cmdRemovePerms}},
        {"listusers", {ACLFlags.value("MODIFY_USERS"), 0, &AOClient::cmdListUsers}},
        {"logout", {ACLFlags.value("NONE"), 0, &AOClient::cmdLogout}},
        {"pos", {ACLFlags.value("NONE"), 1, &AOClient::cmdPos}},
        {"g", {ACLFlags.value("NONE"), 1, &AOClient::cmdG}},
        {"need", {ACLFlags.value("NONE"), 1, &AOClient::cmdNeed}},
        {"coinflip", {ACLFlags.value("NONE"), 0, &AOClient::cmdFlip}},
        {"roll", {ACLFlags.value("NONE"), 0, &AOClient::cmdRoll}},
        {"r", {ACLFlags.value("NONE"), 0, &AOClient::cmdRoll}},
        {"rollp", {ACLFlags.value("NONE"), 0, &AOClient::cmdRollP}},
        {"doc", {ACLFlags.value("NONE"), 0, &AOClient::cmdDoc}},
        {"cleardoc", {ACLFlags.value("NONE"), 0, &AOClient::cmdClearDoc}},
        {"cm", {ACLFlags.value("NONE"), 0, &AOClient::cmdCM}},
        {"uncm", {ACLFlags.value("CM"), 0, &AOClient::cmdUnCM}},
        {"invite", {ACLFlags.value("CM"), 1, &AOClient::cmdInvite}},
        {"uninvite", {ACLFlags.value("CM"), 1, &AOClient::cmdUnInvite}},
        {"lock", {ACLFlags.value("CM"), 0, &AOClient::cmdLock}},
        {"area_lock", {ACLFlags.value("CM"), 0, &AOClient::cmdLock}},
        {"spectatable", {ACLFlags.value("CM"), 0, &AOClient::cmdSpectatable}},
        {"area_spectate", {ACLFlags.value("CM"), 0, &AOClient::cmdSpectatable}},
        {"unlock", {ACLFlags.value("CM"), 0, &AOClient::cmdUnLock}},
        {"area_unlock", {ACLFlags.value("CM"), 0, &AOClient::cmdUnLock}},
        {"timer", {ACLFlags.value("CM"), 0, &AOClient::cmdTimer}},
        {"area", {ACLFlags.value("NONE"), 1, &AOClient::cmdArea}},
        {"play", {ACLFlags.value("CM"), 1, &AOClient::cmdPlay}},
        {"areakick", {ACLFlags.value("CM"), 1, &AOClient::cmdAreaKick}},
        {"area_kick", {ACLFlags.value("CM"), 1, &AOClient::cmdAreaKick}},
        {"randomchar", {ACLFlags.value("NONE"), 0, &AOClient::cmdRandomChar}},
        {"switch", {ACLFlags.value("NONE"), 1, &AOClient::cmdSwitch}},
        {"toggleglobal", {ACLFlags.value("NONE"), 0, &AOClient::cmdToggleGlobal}},
        {"mods", {ACLFlags.value("NONE"), 0, &AOClient::cmdMods}},
        {"commands", {ACLFlags.value("NONE"), 0, &AOClient::cmdCommands}},
        {"status", {ACLFlags.value("NONE"), 1, &AOClient::cmdStatus}},
        {"forcepos", {ACLFlags.value("CM"), 2, &AOClient::cmdForcePos}},
        {"currentmusic", {ACLFlags.value("NONE"), 0, &AOClient::cmdCurrentMusic}},
        {"pm", {ACLFlags.value("NONE"), 2, &AOClient::cmdPM}},
        {"evidence_mod", {ACLFlags.value("EVI_MOD"), 1, &AOClient::cmdEvidenceMod}},
        {"motd", {ACLFlags.value("NONE"), 0, &AOClient::cmdMOTD}},
        {"announce", {ACLFlags.value("ANNOUNCE"), 1, &AOClient::cmdAnnounce}},
        {"m", {ACLFlags.value("MODCHAT"), 1, &AOClient::cmdM}},
        {"gm", {ACLFlags.value("MODCHAT"), 1, &AOClient::cmdGM}},
        {"mute", {ACLFlags.value("MUTE"), 1, &AOClient::cmdMute}},
        {"unmute", {ACLFlags.value("MUTE"), 1, &AOClient::cmdUnMute}},
        {"bans", {ACLFlags.value("BAN"), 0, &AOClient::cmdBans}},
        {"unban", {ACLFlags.value("BAN"), 1, &AOClient::cmdUnBan}},
        {"removeuser", {ACLFlags.value("MODIFY_USERS"), 1, &AOClient::cmdRemoveUser}},
        {"subtheme", {ACLFlags.value("CM"), 1, &AOClient::cmdSubTheme}},
        {"about", {ACLFlags.value("NONE"), 0, &AOClient::cmdAbout}},
        {"evidence_swap", {ACLFlags.value("CM"), 2, &AOClient::cmdEvidence_Swap}},
        {"notecard", {ACLFlags.value("NONE"), 1, &AOClient::cmdNoteCard}},
        {"notecardreveal", {ACLFlags.value("CM"), 0, &AOClient::cmdNoteCardReveal}},
        {"notecard_reveal", {ACLFlags.value("CM"), 0, &AOClient::cmdNoteCardReveal}},
        {"notecardclear", {ACLFlags.value("NONE"), 0, &AOClient::cmdNoteCardClear}},
        {"notecard_clear", {ACLFlags.value("NONE"), 0, &AOClient::cmdNoteCardClear}},
        {"8ball", {ACLFlags.value("NONE"), 1, &AOClient::cmd8Ball}},
        {"lm", {ACLFlags.value("MODCHAT"), 1, &AOClient::cmdLM}},
        {"judgelog", {ACLFlags.value("CM"), 0, &AOClient::cmdJudgeLog}},
        {"allowblankposting", {ACLFlags.value("MODCHAT"), 0, &AOClient::cmdAllowBlankposting}},
        {"allow_blankposting", {ACLFlags.value("MODCHAT"), 0, &AOClient::cmdAllowBlankposting}},
        {"gimp", {ACLFlags.value("MUTE"), 1, &AOClient::cmdGimp}},
        {"ungimp", {ACLFlags.value("MUTE"), 1, &AOClient::cmdUnGimp}},
        {"baninfo", {ACLFlags.value("BAN"), 1, &AOClient::cmdBanInfo}},
        {"testify", {ACLFlags.value("CM"), 0, &AOClient::cmdTestify}},
        {"testimony", {ACLFlags.value("NONE"), 0, &AOClient::cmdTestimony}},
        {"examine", {ACLFlags.value("CM"), 0, &AOClient::cmdExamine}},
        {"pause", {ACLFlags.value("CM"), 0, &AOClient::cmdPauseTestimony}},
        {"delete", {ACLFlags.value("CM"), 0, &AOClient::cmdDeleteStatement}},
        {"update", {ACLFlags.value("CM"), 0, &AOClient::cmdUpdateStatement}},
        {"add", {ACLFlags.value("CM"), 0, &AOClient::cmdAddStatement}},
        {"reload", {ACLFlags.value("SUPER"), 0, &AOClient::cmdReload}},
        {"disemvowel", {ACLFlags.value("MUTE"), 1, &AOClient::cmdDisemvowel}},
        {"undisemvowel", {ACLFlags.value("MUTE"), 1, &AOClient::cmdUnDisemvowel}},
        {"shake", {ACLFlags.value("MUTE"), 1, &AOClient::cmdShake}},
        {"unshake", {ACLFlags.value("MUTE"), 1, &AOClient::cmdUnShake}},
        {"forceimmediate", {ACLFlags.value("CM"), 0, &AOClient::cmdForceImmediate}},
        {"force_noint_pres", {ACLFlags.value("CM"), 0, &AOClient::cmdForceImmediate}},
        {"allowiniswap", {ACLFlags.value("CM"), 0, &AOClient::cmdAllowIniswap}},
        {"allow_iniswap", {ACLFlags.value("CM"), 0, &AOClient::cmdAllowIniswap}},
        {"afk", {ACLFlags.value("NONE"), 0, &AOClient::cmdAfk}},
        {"savetestimony", {ACLFlags.value("NONE"), 1, &AOClient::cmdSaveTestimony}},
        {"loadtestimony", {ACLFlags.value("CM"), 1, &AOClient::cmdLoadTestimony}},
        {"permitsaving", {ACLFlags.value("MODCHAT"), 1, &AOClient::cmdPermitSaving}},
        {"mutepm", {ACLFlags.value("NONE"), 0, &AOClient::cmdMutePM}},
        {"toggleadverts", {ACLFlags.value("NONE"), 0, &AOClient::cmdToggleAdverts}},
        {"oocmute", {ACLFlags.value("MUTE"), 1, &AOClient::cmdOocMute}},
        {"ooc_mute", {ACLFlags.value("MUTE"), 1, &AOClient::cmdOocMute}},
        {"oocunmute", {ACLFlags.value("MUTE"), 1, &AOClient::cmdOocUnMute}},
        {"ooc_unmute", {ACLFlags.value("MUTE"), 1, &AOClient::cmdOocUnMute}},
        {"blockwtce", {ACLFlags.value("MUTE"), 1, &AOClient::cmdBlockWtce}},
        {"block_wtce", {ACLFlags.value("MUTE"), 1, &AOClient::cmdBlockWtce}},
        {"unblockwtce", {ACLFlags.value("MUTE"), 1, &AOClient::cmdUnBlockWtce}},
        {"unblock_wtce", {ACLFlags.value("MUTE"), 1, &AOClient::cmdUnBlockWtce}},
        {"blockdj", {ACLFlags.value("MUTE"), 1, &AOClient::cmdBlockDj}},
        {"block_dj", {ACLFlags.value("MUTE"), 1, &AOClient::cmdBlockDj}},
        {"unblockdj", {ACLFlags.value("MUTE"), 1, &AOClient::cmdUnBlockDj}},
        {"unblock_dj", {ACLFlags.value("MUTE"), 1, &AOClient::cmdUnBlockDj}},
        {"charcurse", {ACLFlags.value("MUTE"), 1, &AOClient::cmdCharCurse}},
        {"uncharcurse", {ACLFlags.value("MUTE"), 1, &AOClient::cmdUnCharCurse}},
        {"charselect", {ACLFlags.value("NONE"), 0, &AOClient::cmdCharSelect}},
        {"togglemusic", {ACLFlags.value("CM"), 0, &AOClient::cmdToggleMusic}},
        {"a", {ACLFlags.value("NONE"), 2, &AOClient::cmdA}},
        {"s", {ACLFlags.value("NONE"), 0, &AOClient::cmdS}},
        {"kickuid", {ACLFlags.value("KICK"), 2, &AOClient::cmdKickUid}},
        {"kick_uid", {ACLFlags.value("KICK"), 2, &AOClient::cmdKickUid}},
        {"firstperson", {ACLFlags.value("NONE"), 0, &AOClient::cmdFirstPerson}},
        {"updateban", {ACLFlags.value("BAN"), 3, &AOClient::cmdUpdateBan}},
        {"update_ban", {ACLFlags.value("BAN"), 3, &AOClient::cmdUpdateBan}},
        {"changepass", {ACLFlags.value("NONE"), 1, &AOClient::cmdChangePassword}},
        {"ignorebglist", {ACLFlags.value("IGNORE_BGLIST"), 0, &AOClient::cmdIgnoreBgList}},
        {"ignore_bglist", {ACLFlags.value("IGNORE_BGLIST"), 0, &AOClient::cmdIgnoreBgList}},
        {"notice", {ACLFlags.value("SEND_NOTICE"), 1, &AOClient::cmdNotice}},
        {"noticeg", {ACLFlags.value("SEND_NOTICE"), 1, &AOClient::cmdNoticeGlobal}},
        {"togglejukebox", {ACLFlags.value("None"), 0, &AOClient::cmdToggleJukebox}},
        {"help", {ACLFlags.value("NONE"), 1, &AOClient::cmdHelp}},
        {"clearcm", {ACLFlags.value("KICK"), 0, &AOClient::cmdClearCM}},
        {"togglemessage", {ACLFlags.value("CM"), 0, &AOClient::cmdToggleAreaMessageOnJoin}},
        {"clearmessage", {ACLFlags.value("CM"), 0, &AOClient::cmdClearAreaMessage}},
        {"areamessage", {ACLFlags.value("CM"), 0, &AOClient::cmdAreaMessage}},
        {"addsong", {ACLFlags.value("CM"), 1, &AOClient::cmdAddSong}},
        {"addcategory", {ACLFlags.value("CM"), 1, &AOClient::cmdAddCategory}},
        {"removeentry", {ACLFlags.value("CM"), 1, &AOClient::cmdRemoveCategorySong}},
        {"toggleroot", {ACLFlags.value("CM"), 0, &AOClient::cmdToggleRootlist}},
        {"clearcustom", {ACLFlags.value("CM"), 0, &AOClient::cmdClearCustom}}};

    /**
     * @brief Filled with part of a packet if said packet could not be read fully from the client's socket.
     *
     * @details Per AO2's network protocol, a packet is finished with the character `%`.
     *
     * @see #is_partial
     */
    QString partial_packet;

    /**
     * @brief True when the previous `readAll()` call from the client's socket returned an unfinished packet.
     *
     * @see #partial_packet
     */
    bool is_partial;

    /**
     * @brief The hardware ID of the client.
     *
     * @details Generated based on the client's own supplied hardware ID.
     * The client supplied hardware ID is generally a machine unique ID.
     */
    QString m_hwid;

    /**
     * @brief The IPID of the client.
     *
     * @details Generated based on the client's IP, but cannot be reversed to identify the client's IP.
     */
    QString m_ipid;

    /**
     * @brief The time in seconds since the client last sent a Witness Testimony / Cross Examination
     * popup packet.
     *
     * @details Used to filter out potential spam.
     */
    long m_last_wtce_time;

    /**
     * @brief The text of the last in-character message that was sent by the client.
     *
     * @details Used to determine if the incoming message is a duplicate.
     */
    QString m_last_message;

    /**
     * @brief Pointer to the servers music manager instance.
     */
    MusicManager *m_music_manager;

    /**
     * @brief A helper function to add recorded packets to an area's judgelog.
     *
     * @param area Pointer to the area where the packet was sent.
     *
     * @param client Pointer to the client that sent the packet.
     *
     * @param action String containing the info that is being recorded.
     */
    void updateJudgeLog(AreaData *area, AOClient *client, QString action);

    /**
     * @brief A helper function for decoding AO encoding from a QString.
     *
     * @param incoming_message QString to be decoded.
     */
    QString decodeMessage(QString incoming_message);

    /**
     * @brief The size, in bytes, of the last data the client sent to the server.
     */
    int last_read = 0;

    /**
     * @brief A helper function for logging in a client as moderator.
     *
     * @param message The OOC message the client has sent.
     */
    void loginAttempt(QString message);

  signals:

    /**
     * @brief Signal connected to universal logger. Sends IC chat usage to the logger.
     */
    void logIC(const QString &f_charName, const QString &f_oocName, const QString &f_ipid,
               const QString &f_areaName, const QString &f_message);

    /**
     * @brief Signal connected to universal logger. Sends OOC chat usage to the logger.
     */
    void logOOC(const QString &f_charName, const QString &f_oocName, const QString &f_ipid,
                const QString &f_areaName, const QString &f_message);

    /**
     * @brief Signal connected to universal logger. Sends login attempt to the logger.
     */
    void logLogin(const QString &f_charName, const QString &f_oocName, const QString &f_moderatorName,
                  const QString &f_ipid, const QString &f_areaName, const bool &f_success);

    /**
     * @brief Signal connected to universal logger. Sends command usage to the logger.
     */
    void logCMD(const QString &f_charName, const QString &f_ipid, const QString &f_oocName, const QString f_command,
                const QStringList f_args, const QString f_areaName);

    /**
     * @brief Signal connected to universal logger. Sends player kick information to the logger.
     */
    void logKick(const QString &f_moderator, const QString &f_targetIPID, const QString &f_reason);

    /**
     * @brief Signal connected to universal logger. Sends ban information to the logger.
     */
    void logBan(const QString &f_moderator, const QString &f_targetIPID, const QString &f_duration, const QString &f_reason);

    /**
     * @brief Signal connected to universal logger. Sends modcall information to the logger, triggering a write of the buffer
     *        when modcall logging is used.
     */
    void logModcall(const QString &f_charName, const QString &f_ipid, const QString &f_oocName, const QString &f_areaName);

    /**
     * @brief Signals the server that the client has disconnected and marks its userID as free again.
     */
    void clientSuccessfullyDisconnected(const int &f_user_id);
};

#endif // AOCLIENT_H
