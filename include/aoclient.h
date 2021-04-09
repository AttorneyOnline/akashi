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

#include "include/aopacket.h"
#include "include/server.h"
#include "include/area_data.h"
#include "include/db_manager.h"

#include <algorithm>

#include <QHostAddress>
#include <QTcpSocket>
#include <QDateTime>
#include <QRegularExpression>
#include <QtGlobal>
#if QT_VERSION > QT_VERSION_CHECK(5, 10, 0)
#include <QRandomGenerator>
#endif

class Server;

/**
 * @brief Represents a client connected to the server running Attorney Online 2 or one of its derivatives.
 */
class AOClient : public QObject {
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
    AOClient(Server* p_server, QTcpSocket* p_socket, QObject* parent = nullptr, int user_id = 0)
        : QObject(parent), id(user_id), remote_ip(p_socket->peerAddress()), password(""),
          joined(false), current_area(0), current_char(""), socket(p_socket), server(p_server),
          is_partial(false), last_wtce_time(0) {};

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
    QString getIpid();

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
    Server* getServer();

    /**
     * @brief The user ID of the client.
     */
    int id;

    /**
     * @brief The IP address of the client.
     */
    QHostAddress remote_ip;

    /**
     * @brief The stored character password for the client, used to be able to select passworded characters.
     */
    QString password;

    /**
     * @brief True if the client is actually in the server.
     *
     * @details To explain: In AO, clients immediately establish connection to the server when the user clicks on the server's name in the server
     * browser. Thus, as the user browses servers, they constantly connect and disconnect to and from servers.
     *
     * The purpose of this variable is to determine if the user isn't just doing that, but has actually double-clicked the server, and
     * its client has sent the standard handshake packets, which does signify that the client intended to 'join' this server.
     */
    bool joined;

    /**
     * @brief The ID of the area the client is currently in.
     */
    int current_area;

    /**
     * @brief The internal name of the character the client is currently using.
     */
    QString current_char;

    /**
     * @brief The internal name of the character the client is iniswapped to.
     *
     * @note This will be the same as current_char if the client is not iniswapped.
     */
    QString current_iniswap;

    /**
     * @brief If true, the client is a logged-in moderator.
     */
    bool authenticated = false;

    /**
     * @brief If using advanced authentication, this is the moderator name that the client has logged in with.
     */
    QString moderator_name = "";

    /**
     * @brief The out-of-character name of the client, generally the nickname of the user themself.
     */
    QString ooc_name = "";

    /**
     * @brief The custom showname of the client, used when "renaming" already existing characters in-character.
     */
    QString showname = "";

    /**
     * @brief If true, the client is willing to receive global messages.
     *
     * @see AOClient::cmdG and AOClient::cmdToggleGlobal
     */
    bool global_enabled = true;

    /**
     * @brief If true, the client may not use in-character chat.
     */
    bool is_muted = false;
  
    /**
     * @brief If true, the client may not use out-of-character chat.
     */
    bool is_ooc_muted = false;
  
    /**
     * @brief If true, the client may not use the music list.
     */
    bool is_dj_blocked = false;
  
    /**
     * @brief If true, the client may not use the judge controls.
     */
    bool is_wtce_blocked = false;
    
    /**
     * @brief Represents the client's client software, and its version.
     *
     * @note Though the version number and naming scheme looks vaguely semver-like,
     * do not be misled into thinking it is that.
     */
    struct ClientVersion {
      QString string; //!< The name of the client software, for example, `AO2`.
      int release = -1; //!< The 'release' part of the version number. In Attorney Online's case, this is fixed at `2`.
      int major = -1; //!< The 'major' part of the version number. In Attorney Online's case, this increases when a new feature is introduced (generally).
      int minor = -1; //!< The 'minor' part of the version number. In Attorney Online's case, this increases for bugfix releases (generally).
    };

    /**
     * @brief The software and version of the client.
     *
     * @see The struct itself for more details.
     */
    ClientVersion version;

    /**
      * @brief The authorisation bitflag, representing what permissions a client can have.
      *
      * @showinitializer
      */
    QMap<QString, unsigned long long> ACLFlags {
        {"NONE",            0ULL      },
        {"KICK",            1ULL << 0 },
        {"BAN",             1ULL << 1 },
        {"BGLOCK",          1ULL << 2 },
        {"MODIFY_USERS",    1ULL << 3 },
        {"CM",              1ULL << 4 },
        {"GLOBAL_TIMER",    1ULL << 5 },
        {"EVI_MOD",         1ULL << 6 },
        {"MOTD",            1ULL << 7 },
        {"ANNOUNCE",        1ULL << 8 },
        {"MODCHAT",         1ULL << 9 },
        {"MUTE",            1ULL << 10},
        {"SUPER",          ~0ULL      },
    };

    /**
     * @brief If true, the client's in-character messages will have their word order randomised.
     */
    bool is_shaken;

    /**
     * @brief If true, the client's in-character messages will have their vowels (English alphabet only) removed.
     */
    bool is_disemvoweled;

    /**
     * @brief If true, the client's in-character messages will be overwritten by a randomly picked predetermined message.
     */
    bool is_gimped;

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

  private:
    /**
     * @brief The TCP socket used to communicate with the client.
     */
    QTcpSocket* socket;

    /**
     * @brief A pointer to the Server, used for updating server variables that depend on the client (e.g. amount of players in an area).
     */
    Server* server;

    /**
     * @brief The type of area update, used for area update (ARUP) packets.
     */
    enum ARUPType {
        PLAYER_COUNT, //!< The packet contains player count updates.
        STATUS, //!< The packet contains area status updates.
        CM, //!< The packet contains updates about who's the CM of what area.
        LOCKED //!< The packet contains updates about what areas are locked.
    };

    /**
     * @brief Used for the common parts of the dice rolling commands, to determine where the function should go after the common functionality.
     *
     * @see AOClient::diceThrower
     */
    enum RollType {
        ROLL, //!< The roll is a simple numerical roll, should be announced in the area.
        ROLLP, //!< The roll is a numerical roll, but private, the result should only be told to the caller.
        ROLLA //!< The roll is an ability roll, the values must be read out of the ability die configs.
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
     * @brief Checks if the client would be authorised to something based on its necessary permissions.
     *
     * @param acl_mask The permissions bitflag that the client's own permissions should be checked against.
     *
     * @return True if the client's permissions are high enough for `acl_mask`, or higher than it.
     * False if the client is missing some permissions.
     */
    bool checkAuth(unsigned long long acl_mask);

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
    void pktDefault(AreaData* area, int argc, QStringList argv, AOPacket packet);

    /// Implements [hardware ID](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#hard-drive-id).
    void pktHardwareId(AreaData* area, int argc, QStringList argv, AOPacket packet);

    /**
     * @brief Implements [feature list](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#feature-list) and
     * [player count](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#player-count).
     */
    void pktSoftwareId(AreaData* area, int argc, QStringList argv, AOPacket packet);

    /// Implements [resource counts](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#resource-counts).
    void pktBeginLoad(AreaData* area, int argc, QStringList argv, AOPacket packet);

    /// Implements [character list](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#character-list).
    void pktRequestChars(AreaData* area, int argc, QStringList argv, AOPacket packet);

    /// Implements [music list](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#music-list).
    void pktRequestMusic(AreaData* area, int argc, QStringList argv, AOPacket packet);

    /// Implements [the final loading confirmation](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#final-confirmation).
    void pktLoadingDone(AreaData* area, int argc, QStringList argv, AOPacket packet);

    /**
      * @brief Implements character passwording. This is not on the netcode documentation as of writing.
      *
      * @todo Link packet details when it gets into the netcode documentation.
      */
    void pktCharPassword(AreaData* area, int argc, QStringList argv, AOPacket packet);

    /// Implements [character selection](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#choose-character).
    void pktSelectChar(AreaData* area, int argc, QStringList argv, AOPacket packet);

    /// Implements [the in-character messaging hell](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#in-character-message).
    void pktIcChat(AreaData* area, int argc, QStringList argv, AOPacket packet);

    /// Implements [out-of-character messages](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#out-of-character-message).
    void pktOocChat(AreaData* area, int argc, QStringList argv, AOPacket packet);

    /// Implements [the keepalive packet](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#keep-alive).
    void pktPing(AreaData* area, int argc, QStringList argv, AOPacket packet);

    /**
      * @brief Implements [music](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#music) and
      * [area changing](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#switch-area).
      */
    void pktChangeMusic(AreaData* area, int argc, QStringList argv, AOPacket packet);


    /**
      * @brief Implements [the witness testimony / cross examination / judge decision popups]
      * (https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#witness-testimonycross-examination-wtce).
      */
    void pktWtCe(AreaData* area, int argc, QStringList argv, AOPacket packet);

    /// Implements [penalty bars](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#penalty-health-bars).
    void pktHpBar(AreaData* area, int argc, QStringList argv, AOPacket packet);

    /**
      * @brief Implements WebSocket IP handling. This is not on the netcode documentation as of writing.
      *
      * @todo Link packet details when it gets into the netcode documentation.
      */
    void pktWebSocketIp(AreaData* area, int argc, QStringList argv, AOPacket packet);

    /// Implements [moderator calling](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#call-mod).
    void pktModCall(AreaData* area, int argc, QStringList argv, AOPacket packet);

    /// Implements [adding evidence](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#add).
    void pktAddEvidence(AreaData* area, int argc, QStringList argv, AOPacket packet);

    /// Implements [removing evidence](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#remove).
    void pktRemoveEvidence(AreaData* area, int argc, QStringList argv, AOPacket packet);

    /// Implements [editing evidence](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#edit).
    void pktEditEvidence(AreaData* area, int argc, QStringList argv, AOPacket packet);

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
    void sendEvidenceList(AreaData* area);

    /**
     * @brief Updates the evidence list in the area for the client.
     *
     * @param area The client's area.
     */
    void updateEvidenceList(AreaData* area);

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
    bool checkEvidenceAccess(AreaData* area);

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
    int char_id = -1;

    /**
     * @brief The character ID of the other character that the client wants to pair up with.
     *
     * @details Though this uses character ID, a client with *that* character ID must exist in the area for the pairing to work.
     * Furthermore, the owner of that character ID must also do the reverse to this client, making their `pairing_with` equal
     * to this client's character ID.
     */
    int pairing_with = -1;

    /**
     * @brief The name of the emote last used by the client. No extension.
     *
     * @details This is used for pairing mainly, for the server to be able to craft a smooth-looking transition from one
     * paired-up client talking to the next.
     */
    QString emote = "";

    /**
     * @brief The amount the client was last offset by.
     *
     * @details This used to be just a plain number ranging from -100 to 100, but then Crystal mangled it by building some extra data into it.
     * Cheers, love.
     */
    QString offset = "";

    /**
     * @brief The last flipped state of the client.
     */
    QString flipping = "";

    /**
     * @brief The last reported position of the client.
     */
    QString pos = "";

    ///@}

    /// Describes a packet's interpretation details.
    struct PacketInfo {
        unsigned long long acl_mask; //!< The permissions necessary for the packet.
        int minArgs; //!< The minimum arguments needed for the packet to be interpreted correctly / make sense.
        void (AOClient::*action)(AreaData*, int, QStringList, AOPacket);
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
    const QMap<QString, PacketInfo> packets {
        {"HI",      {ACLFlags.value("NONE"), 1,  &AOClient::pktHardwareId     }},
        {"ID",      {ACLFlags.value("NONE"), 2,  &AOClient::pktSoftwareId     }},
        {"askchaa", {ACLFlags.value("NONE"), 0,  &AOClient::pktBeginLoad      }},
        {"RC",      {ACLFlags.value("NONE"), 0,  &AOClient::pktRequestChars   }},
        {"RM",      {ACLFlags.value("NONE"), 0,  &AOClient::pktRequestMusic   }},
        {"RD",      {ACLFlags.value("NONE"), 0,  &AOClient::pktLoadingDone    }},
        {"PW",      {ACLFlags.value("NONE"), 1,  &AOClient::pktCharPassword   }},
        {"CC",      {ACLFlags.value("NONE"), 3,  &AOClient::pktSelectChar     }},
        {"MS",      {ACLFlags.value("NONE"), 15, &AOClient::pktIcChat         }},
        {"CT",      {ACLFlags.value("NONE"), 2,  &AOClient::pktOocChat        }},
        {"CH",      {ACLFlags.value("NONE"), 1,  &AOClient::pktPing           }},
        {"MC",      {ACLFlags.value("NONE"), 2,  &AOClient::pktChangeMusic    }},
        {"RT",      {ACLFlags.value("NONE"), 1,  &AOClient::pktWtCe           }},
        {"HP",      {ACLFlags.value("NONE"), 2,  &AOClient::pktHpBar          }},
        {"WSIP",    {ACLFlags.value("NONE"), 1,  &AOClient::pktWebSocketIp    }},
        {"ZZ",      {ACLFlags.value("NONE"), 0,  &AOClient::pktModCall        }},
        {"PE",      {ACLFlags.value("NONE"), 3,  &AOClient::pktAddEvidence    }},
        {"DE",      {ACLFlags.value("NONE"), 1,  &AOClient::pktRemoveEvidence }},
        {"EE",      {ACLFlags.value("NONE"), 4,  &AOClient::pktEditEvidence   }},
    };

    /**
     * @brief Literally just an invalid default command. That's it.
     *
     * @note Can be used as a base for future commands.
     *
     * @iscommand
     */
    void cmdDefault(int argc, QStringList argv);

    /**
     * @brief Lists all the commands that the caller client has the permissions to use.
     *
     * @details No arguments.
     *
     * @iscommand
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
      * @name Authentication
      */
    ///@{

    /**
     * @brief Logs the user in as a moderator.
     *
     * @details If the authorisation type is `"simple"`, then this command expects one argument, the **global moderator password**.
     *
     * If the authorisation type is `"advanced"`, then it requires two arguments, the **moderator's username** and the **matching password**.
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
     * @brief Returns the currently playing music in the area, and who played it.
     *
     * @details No arguments.
     *
     * @iscommand
     */
    void cmdCurrentMusic(int argc, QStringList argv);

    ///@}

    /**
      * @name Moderation
      *
      * @brief All functions that detail the actions of commands,
      * that are also related to the moderation and administration of the server.
      */
    ///@{

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
     * @details The first argument is the **target's IPID**, the second is the **reason** why the client
     * was banned, the third is the **duration**.
     *
     * Both the reason and the duration must be in quotation marks.
     *
     * The duration can be `"perma"`, meaning a forever ban, otherwise, it must be given in the format of `"YYyWWwDDdHHhMMmSSs"` to
     * mean a YY years, WW weeks, DD days, HH hours, MM minutes and SS seconds long ban. Any of these may be left out, for example,
     * `"1h30m"` for a 1.5 hour long ban.
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

    // Casing/RP

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
    void cmdAllow_Blankposting(int argc, QStringList argv);

    ///@}

    /**
      * @name Roleplay
      *
      * @brief All functions that detail the actions of commands,
      * that are also related to various kinds of roleplay actions in some way.
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
     * @brief A global message expressing that the client needs something (generally: players for something).
     *
     * @details The arguments are **the message** that the client wants to send.
     *
     * @iscommand
     */
    void cmdNeed(int argc, QStringList argv);

    /**
     * @brief Flips a coin, returning heads or tails.
     *
     * @details No arguments.
     *
     * @iscommand
     */
    void cmdFlip(int argc, QStringList argv);

    /**
     * @brief Rolls dice, summing the results.
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
  
    // Messaging/Client
    
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
     * Can silently "fail" if the character picked is already being used by another client.
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
     * @brief Randomly selects an answer from 8ball.txt to a question.
     *
     * @details The only argument is the question the client wants answered.
     *
     * @iscommand
     */
    void cmd8Ball(int argc, QStringList argv);

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
     * @brief Returns a textual representation of the time left in an area's Timer.
     *
     * @param area_idx The ID of the area whose timer to grab.
     * @param timer The pointer to the area's timer.
     *
     * @return A textual representation of the time left over on the Timer,
     * or `"Timer is inactive"` if the timer wasn't started.
     */
    QString getAreaTimer(int area_idx, QTimer* timer);

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
     * @brief A convenience function unifying the various dice-rolling methods.
     *
     * @internal
     *  Babby's first code. <3
     * @endinternal
     *
     * @param argc The amount of arguments arriving to the function through a command call,
     * equivalent to `argv.size()`.
     * See @ref commandArgc "CommandInfo's `action`'s first parameter".
     * @param argv The list of arguments passed to the function through a command call.
     * See @ref commandArgv "CommandInfo's `action`'s second parameter".
     * @param Type The type of the dice-rolling being done.
     */
    void diceThrower(int argc, QStringList argv, RollType Type);

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
    QString getReprimand(bool positive = false);

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
    struct CommandInfo {
        unsigned long long acl_mask; //!< The permissions necessary to be able to run the command. @see ACLFlags.
        int minArgs; //!< The minimum mandatory arguments needed for the command to function.
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
    const QMap<QString, CommandInfo> commands {
        {"login",           {ACLFlags.value("NONE"),         1, &AOClient::cmdLogin}},
        {"getareas",        {ACLFlags.value("NONE"),         0, &AOClient::cmdGetAreas}},
        {"getarea",         {ACLFlags.value("NONE"),         0, &AOClient::cmdGetArea}},
        {"ban",             {ACLFlags.value("BAN"),          2, &AOClient::cmdBan}},
        {"kick",            {ACLFlags.value("KICK"),         2, &AOClient::cmdKick}},
        {"changeauth",      {ACLFlags.value("SUPER"),        0, &AOClient::cmdChangeAuth}},
        {"rootpass",        {ACLFlags.value("SUPER"),        1, &AOClient::cmdSetRootPass}},
        {"background",      {ACLFlags.value("NONE"),         1, &AOClient::cmdSetBackground}},
        {"bg",              {ACLFlags.value("NONE"),         1, &AOClient::cmdSetBackground}},
        {"bglock",          {ACLFlags.value("BGLOCK"),       0, &AOClient::cmdBgLock}},
        {"bgunlock",        {ACLFlags.value("BGLOCK"),       0, &AOClient::cmdBgUnlock}},
        {"adduser",         {ACLFlags.value("MODIFY_USERS"), 2, &AOClient::cmdAddUser}},
        {"listperms",       {ACLFlags.value("NONE"),         0, &AOClient::cmdListPerms}},
        {"addperm",         {ACLFlags.value("MODIFY_USERS"), 2, &AOClient::cmdAddPerms}},
        {"removeperm",      {ACLFlags.value("MODIFY_USERS"), 2, &AOClient::cmdRemovePerms}},
        {"listusers",       {ACLFlags.value("MODIFY_USERS"), 0, &AOClient::cmdListUsers}},
        {"logout",          {ACLFlags.value("NONE"),         0, &AOClient::cmdLogout}},
        {"pos",             {ACLFlags.value("NONE"),         1, &AOClient::cmdPos}},
        {"g",               {ACLFlags.value("NONE"),         1, &AOClient::cmdG}},
        {"need",            {ACLFlags.value("NONE"),         1, &AOClient::cmdNeed}},
        {"coinflip",        {ACLFlags.value("NONE"),         0, &AOClient::cmdFlip}},
        {"roll",            {ACLFlags.value("NONE"),         0, &AOClient::cmdRoll}},
        {"rollp",           {ACLFlags.value("NONE"),         0, &AOClient::cmdRollP}},
        {"doc",             {ACLFlags.value("NONE"),         0, &AOClient::cmdDoc}},
        {"cleardoc",        {ACLFlags.value("NONE"),         0, &AOClient::cmdClearDoc}},
        {"cm",              {ACLFlags.value("NONE"),         0, &AOClient::cmdCM}},
        {"uncm",            {ACLFlags.value("CM"),           0, &AOClient::cmdUnCM}},
        {"invite",          {ACLFlags.value("CM"),           1, &AOClient::cmdInvite}},
        {"uninvite",        {ACLFlags.value("CM"),           1, &AOClient::cmdUnInvite}},
        {"lock",            {ACLFlags.value("CM"),           0, &AOClient::cmdLock}},
        {"area_lock",       {ACLFlags.value("CM"),           0, &AOClient::cmdLock}},
        {"spectatable",     {ACLFlags.value("CM"),           0, &AOClient::cmdSpectatable}},
        {"area_spectate",   {ACLFlags.value("CM"),           0, &AOClient::cmdSpectatable}},
        {"unlock",          {ACLFlags.value("CM"),           0, &AOClient::cmdUnLock}},
        {"area_unlock",     {ACLFlags.value("CM"),           0, &AOClient::cmdUnLock}},
        {"timer",           {ACLFlags.value("CM"),           0, &AOClient::cmdTimer}},
        {"area",            {ACLFlags.value("NONE"),         1, &AOClient::cmdArea}},
        {"play",            {ACLFlags.value("CM"),           1, &AOClient::cmdPlay}},
        {"areakick",        {ACLFlags.value("CM"),           1, &AOClient::cmdAreaKick}},
        {"area_kick",       {ACLFlags.value("CM"),           1, &AOClient::cmdAreaKick}},
        {"randomchar",      {ACLFlags.value("NONE"),         0, &AOClient::cmdRandomChar}},
        {"switch",          {ACLFlags.value("NONE"),         1, &AOClient::cmdSwitch}},
        {"toggleglobal",    {ACLFlags.value("NONE"),         0, &AOClient::cmdToggleGlobal}},
        {"mods",            {ACLFlags.value("NONE"),         0, &AOClient::cmdMods}},
        {"help",            {ACLFlags.value("NONE"),         0, &AOClient::cmdHelp}},
        {"status",          {ACLFlags.value("NONE"),         1, &AOClient::cmdStatus}},
        {"forcepos",        {ACLFlags.value("CM"),           2, &AOClient::cmdForcePos}},
        {"currentmusic",    {ACLFlags.value("NONE"),         0, &AOClient::cmdCurrentMusic}},
        {"pm",              {ACLFlags.value("NONE"),         2, &AOClient::cmdPM}},
        {"evidence_mod",    {ACLFlags.value("EVI_MOD"),      1, &AOClient::cmdEvidenceMod}},
        {"motd",            {ACLFlags.value("NONE"),         0, &AOClient::cmdMOTD}},
        {"announce",        {ACLFlags.value("ANNOUNCE"),     1, &AOClient::cmdAnnounce}},
        {"m",               {ACLFlags.value("MODCHAT"),      1, &AOClient::cmdM}},
        {"gm",              {ACLFlags.value("MODCHAT"),      1, &AOClient::cmdGM}},
        {"mute",            {ACLFlags.value("MUTE"),         1, &AOClient::cmdMute}},
        {"unmute",          {ACLFlags.value("MUTE"),         1, &AOClient::cmdUnMute}},
        {"bans",            {ACLFlags.value("BAN"),          0, &AOClient::cmdBans}},
        {"unban",           {ACLFlags.value("BAN"),          1, &AOClient::cmdUnBan}},
        {"removeuser",      {ACLFlags.value("MODIFY_USERS"), 1, &AOClient::cmdRemoveUser}},
        {"subtheme",        {ACLFlags.value("CM"),           1, &AOClient::cmdSubTheme}},
        {"about",           {ACLFlags.value("NONE"),         0, &AOClient::cmdAbout}},
        {"evidence_swap",   {ACLFlags.value("CM"),           2, &AOClient::cmdEvidence_Swap}},
        {"notecard",        {ACLFlags.value("NONE"),         1, &AOClient::cmdNoteCard}},
        {"notecardreveal",  {ACLFlags.value("CM"),           0, &AOClient::cmdNoteCardReveal}},
        {"notecard_reveal", {ACLFlags.value("CM"),           0, &AOClient::cmdNoteCardReveal}},
        {"notecardclear",   {ACLFlags.value("NONE"),         0, &AOClient::cmdNoteCardClear}},
        {"notecard_clear",  {ACLFlags.value("NONE"),         0, &AOClient::cmdNoteCardClear}},
        {"8ball",           {ACLFlags.value("NONE"),         1, &AOClient::cmd8Ball}},
        {"lm",              {ACLFlags.value("MODCHAT"),      1, &AOClient::cmdLM}},
        {"allow_blankposting", {ACLFlags.value("MODCHAT"),      0, &AOClient::cmdAllow_Blankposting}},
    };

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
    QString hwid;

    /**
     * @brief The IPID of the client.
     *
     * @details Generated based on the client's IP, but cannot be reversed to identify the client's IP.
     */
    QString ipid;

    /**
     * @brief The time in seconds since the client last sent a Witness Testimony / Cross Examination
     * popup packet.
     *
     * @details Used to filter out potential spam.
     */
    long last_wtce_time;

    /**
     * @brief The text of the last in-character message that was sent by the client.
     *
     * @details Used to determine if the incoming message is a duplicate.
     */
    QString last_message;
};

#endif // AOCLIENT_H
