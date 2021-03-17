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

/**
  * @defgroup commandHelperFunctions Command helper functions
  *
  * @brief A collection of functions of shared behaviour between command functions,
  * allowing the abstraction of technical details in the command function definition,
  * or the avoidance of repetition over multiple definitions.
  */

/**
  * @defgroup commands Commands
  *
  * @brief These functions all represent actions that would happen if a client called it specifically
  * with a `/command` (with `command` being the command's name) in the out-of-character chat.
  *
  * @details Since all of these functions *are* just command calls, their format is going to be the same.
  * They will have the following parameters:
  *
  * @param argc The amount of arguments arriving to the function through a command call,
  * equivalent to `argv.size()`.
  * See @ref commandArgc "CommandInfo's `action`'s first parameter".
  * @param argv The list of arguments passed to the function through a command call.
  * See @ref commandArgv "CommandInfo's `action`'s second parameter".
  *
  * If documenting a command, you can use the `\@iscommand` alias to quickly add the arguments described
  * above to the function's documentation.
  *
  * @see AOClient::CommandInfo
  */

/**
  * @defgroup commandsMessaging Messaging- and client-related commands
  *
  * @brief All functions that detail the actions of commands,
  * that are also related to messages or the client's self-management in some way.
  *
  * @ingroup commands
  */

/**
  * @defgroup commandsRP Roleplay- and casing-related commands
  *
  * @brief All functions that detail the actions of commands,
  * that are also related to various kinds of roleplay actions in some way.
  *
  * @ingroup commands
  */

#include "include/aopacket.h"
#include "include/server.h"
#include "include/area_data.h"
#include "include/db_manager.h"

#include <algorithm>

#include <QHostAddress>
#include <QTcpSocket>
#include <QDateTime>
#include <QRegExp>
#include <QtGlobal>
#if QT_VERSION > QT_VERSION_CHECK(5, 10, 0)
#include <QRandomGenerator>
#endif

class Server;

class AOClient : public QObject {
    Q_OBJECT
  public:
    AOClient(Server* p_server, QTcpSocket* p_socket, QObject* parent = nullptr, int user_id = 0);
    ~AOClient();

    QString getHwid();
    QString getIpid();
    Server* getServer();
    void setHwid(QString p_hwid);

    int id;

    QHostAddress remote_ip;
    QString password;
    bool joined;
    int current_area;
    QString current_char;
    bool authenticated = false;
    QString moderator_name = "";
    QString ooc_name = "";
    QString showname = "";
    bool global_enabled = true;
    struct ClientVersion {
      QString string;
      int release = -1;
      int major = -1;
      int minor = -1;
    };
    ClientVersion version;

    QMap<QString, unsigned long long> ACLFlags {
        {"KICK",            1ULL << 0},
        {"BAN",             1ULL << 1},
        {"BGLOCK",          1ULL << 2},
        {"MODIFY_USERS",    1ULL << 3},
        {"CM",              1ULL << 4},
        {"GLOBAL_TIMER",    1ULL << 5},
        {"CHANGE_EVI_MOD",  1ULL << 6},
        {"SUPER",          ~0ULL     },
    };

  public slots:
    void clientDisconnected();
    void clientData();
    void sendPacket(AOPacket packet);
    void sendPacket(QString header, QStringList contents);
    void sendPacket(QString header);

  private:
    QTcpSocket* socket;
    Server* server;

    enum ARUPType {
        PLAYER_COUNT,
        STATUS,
        CM,
        LOCKED
    };

    enum RollType {
        ROLL,
        ROLLP,
        ROLLA
    };

    void handlePacket(AOPacket packet);
    void handleCommand(QString command, int argc, QStringList argv);
    void changeArea(int new_area);
    void changeCharacter(int char_id);
    void changePosition(QString new_pos);
    void arup(ARUPType type, bool broadcast);
    void fullArup();
    void sendServerMessage(QString message);
    void sendServerMessageArea(QString message);
    void sendServerBroadcast(QString message);
    bool checkAuth(unsigned long long acl_mask);

    // Packet headers
    void pktDefault(AreaData* area, int argc, QStringList argv, AOPacket packet);
    void pktHardwareId(AreaData* area, int argc, QStringList argv, AOPacket packet);
    void pktSoftwareId(AreaData* area, int argc, QStringList argv, AOPacket packet);
    void pktBeginLoad(AreaData* area, int argc, QStringList argv, AOPacket packet);
    void pktRequestChars(AreaData* area, int argc, QStringList argv, AOPacket packet);
    void pktRequestMusic(AreaData* area, int argc, QStringList argv, AOPacket packet);
    void pktLoadingDone(AreaData* area, int argc, QStringList argv, AOPacket packet);
    void pktCharPassword(AreaData* area, int argc, QStringList argv, AOPacket packet);
    void pktSelectChar(AreaData* area, int argc, QStringList argv, AOPacket packet);
    void pktIcChat(AreaData* area, int argc, QStringList argv, AOPacket packet);
    void pktOocChat(AreaData* area, int argc, QStringList argv, AOPacket packet);
    void pktPing(AreaData* area, int argc, QStringList argv, AOPacket packet);
    void pktChangeMusic(AreaData* area, int argc, QStringList argv, AOPacket packet);
    void pktWtCe(AreaData* area, int argc, QStringList argv, AOPacket packet);
    void pktHpBar(AreaData* area, int argc, QStringList argv, AOPacket packet);
    void pktWebSocketIp(AreaData* area, int argc, QStringList argv, AOPacket packet);
    void pktModCall(AreaData* area, int argc, QStringList argv, AOPacket packet);
    void pktAddEvidence(AreaData* area, int argc, QStringList argv, AOPacket packet);
    void pktRemoveEvidence(AreaData* area, int argc, QStringList argv, AOPacket packet);
    void pktEditEvidence(AreaData* area, int argc, QStringList argv, AOPacket packet);

    // Packet helper functions
    void sendEvidenceList(AreaData* area);
    void updateEvidenceList(AreaData* area);
    AOPacket validateIcPacket(AOPacket packet);
    QString dezalgo(QString p_text);
    bool checkEvidenceAccess(AreaData* area);

    // Packet helper global variables
    int char_id = -1;
    int pairing_with = -1;
    QString emote = "";
    QString offset = "";
    QString flipping = "";
    QString pos = "";

    struct PacketInfo {
        unsigned long long acl_mask;
        int minArgs;
        void (AOClient::*action)(AreaData*, int, QStringList, AOPacket);
    };

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

    //// Commands
    void cmdDefault(int argc, QStringList argv);
    void cmdHelp(int argc, QStringList argv);
    // Authentication
    void cmdLogin(int argc, QStringList argv);
    void cmdChangeAuth(int argc, QStringList argv);
    void cmdSetRootPass(int argc, QStringList argv);
    void cmdAddUser(int argc, QStringList argv);
    void cmdListPerms(int argc, QStringList argv);
    void cmdAddPerms(int argc, QStringList argv);
    void cmdRemovePerms(int argc, QStringList argv);
    void cmdListUsers(int argc, QStringList argv);
    void cmdLogout(int argc, QStringList argv);
    // Areas
    void cmdCM(int argc, QStringList argv);
    void cmdUnCM(int argc, QStringList argv);
    void cmdInvite(int argc, QStringList argv);
    void cmdUnInvite(int argc, QStringList argv);
    void cmdLock(int argc, QStringList argv);
    void cmdSpectatable(int argc, QStringList argv);
    void cmdUnLock(int argc, QStringList argv);
    void cmdGetAreas(int argc, QStringList argv);
    void cmdGetArea(int argc, QStringList argv);
    void cmdArea(int argc, QStringList argv);
    void cmdAreaKick(int argc, QStringList argv);
    void cmdSetBackground(int argc, QStringList argv);
    void cmdBgLock(int argc, QStringList argv);
    void cmdBgUnlock(int argc, QStringList argv);
    void cmdStatus(int argc, QStringList argv);
    void cmdCurrentMusic(int argc, QStringList argv);
    // Moderation
    void cmdMods(int argc, QStringList argv);
    void cmdBan(int argc, QStringList argv);
    void cmdKick(int argc, QStringList argv);
    // Casing/RP
    void cmdPlay(int argc, QStringList argv);
    void cmdNeed(int argc, QStringList argv);
    void cmdFlip(int argc, QStringList argv);
    void cmdRoll(int argc, QStringList argv);
    void cmdRollP(int argc, QStringList argv);
    void cmdDoc(int argc, QStringList argv);
    void cmdClearDoc(int argc, QStringList argv);
    void cmdTimer(int argc, QStringList argv);

    /**
     * @brief Changes the evidence mod in the area.
     *
     * @details The only argument is the **evidence mod** to change to.
     *
     * @iscommand
     *
     * @see AreaData::EvidenceMod
     * @ingroup commandsRP
     */
    void cmdEvidenceMod(int argc, QStringList argv);

    /**
     * @brief Changes the client's position.
     *
     * @details The only argument is the **target position** to move the client to.
     *
     * @iscommand
     * @ingroup commandsMessaging
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
     * @ingroup commandsMessaging
     */
    void cmdForcePos(int argc, QStringList argv);

    /**
     * @brief Switches to a different character based on character ID.
     *
     * @details The only argument is the **character's ID** that the client wants to switch to.
     *
     * @iscommand
     * @ingroup commandsMessaging
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
     * @ingroup commandsMessaging
     */
    void cmdRandomChar(int argc, QStringList argv);

    /**
     * @brief Sends a global message (i.e., all clients in the server will be able to see it).
     *
     * @details The arguments are **the message** that the client wants to send.
     *
     * @iscommand
     * @ingroup commandsMessaging
     */
    void cmdG(int argc, QStringList argv);

    /**
     * @brief Toggles whether the client will ignore @ref cmdG "global" messages or not.
     *
     * @details No arguments.
     *
     * @iscommand
     * @ingroup commandsMessaging
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
     * @ingroup commandsMessaging
     */
    void cmdPM(int argc, QStringList argv);

    /**
     * @brief Returns a textual representation of the time left in an area's Timer.
     *
     * @param area_idx The ID of the area whose timer to grab.
     * @param timer The pointer to the area's timer.
     *
     * @return A textual representation of the time left over on the Timer,
     * or `"Timer is inactive"` if the timer wasn't started.
     *
     * @ingroup commandHelperFunctions
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
     *
     * @ingroup commandHelperFunctions
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
     *
     * @ingroup commandHelperFunctions
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
     *
     * @ingroup commandHelperFunctions
     */
    void diceThrower(int argc, QStringList argv, RollType Type);

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
        unsigned long long acl_mask; //!< The privileges necessary to be able to run the command. @see ACLFlags.
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
        {"login",         {ACLFlags.value("NONE"),           1, &AOClient::cmdLogin         }},
        {"getareas",      {ACLFlags.value("NONE"),           0, &AOClient::cmdGetAreas      }},
        {"getarea",       {ACLFlags.value("NONE"),           0, &AOClient::cmdGetArea       }},
        {"ban",           {ACLFlags.value("BAN"),            2, &AOClient::cmdBan           }},
        {"kick",          {ACLFlags.value("KICK"),           2, &AOClient::cmdKick          }},
        {"changeauth",    {ACLFlags.value("SUPER"),          0, &AOClient::cmdChangeAuth    }},
        {"rootpass",      {ACLFlags.value("SUPER"),          1, &AOClient::cmdSetRootPass   }},
        {"background",    {ACLFlags.value("NONE"),           1, &AOClient::cmdSetBackground }},
        {"bg",            {ACLFlags.value("NONE"),           1, &AOClient::cmdSetBackground }},
        {"bglock",        {ACLFlags.value("BGLOCK"),         0, &AOClient::cmdBgLock        }},
        {"bgunlock",      {ACLFlags.value("BGLOCK"),         0, &AOClient::cmdBgUnlock      }},
        {"adduser",       {ACLFlags.value("MODIFY_USERS"),   2, &AOClient::cmdAddUser       }},
        {"listperms",     {ACLFlags.value("NONE"),           0, &AOClient::cmdListPerms     }},
        {"addperm",       {ACLFlags.value("MODIFY_USERS"),   2, &AOClient::cmdAddPerms      }},
        {"removeperm",    {ACLFlags.value("MODIFY_USERS"),   2, &AOClient::cmdRemovePerms   }},
        {"listusers",     {ACLFlags.value("MODIFY_USERS"),   0, &AOClient::cmdListUsers     }},
        {"logout",        {ACLFlags.value("NONE"),           0, &AOClient::cmdLogout        }},
        {"pos",           {ACLFlags.value("NONE"),           1, &AOClient::cmdPos           }},
        {"g",             {ACLFlags.value("NONE"),           1, &AOClient::cmdG             }},
        {"need",          {ACLFlags.value("NONE"),           1, &AOClient::cmdNeed          }},
        {"coinflip",      {ACLFlags.value("NONE"),           0, &AOClient::cmdFlip          }},
        {"roll",          {ACLFlags.value("NONE"),           0, &AOClient::cmdRoll          }},
        {"rollp",         {ACLFlags.value("NONE"),           0, &AOClient::cmdRollP         }},
        {"doc",           {ACLFlags.value("NONE"),           0, &AOClient::cmdDoc           }},
        {"cleardoc",      {ACLFlags.value("NONE"),           0, &AOClient::cmdClearDoc      }},
        {"cm",            {ACLFlags.value("NONE"),           0, &AOClient::cmdCM            }},
        {"uncm",          {ACLFlags.value("CM"),             0, &AOClient::cmdUnCM          }},
        {"invite",        {ACLFlags.value("CM"),             1, &AOClient::cmdInvite        }},
        {"uninvite",      {ACLFlags.value("CM"),             1, &AOClient::cmdUnInvite      }},
        {"lock",          {ACLFlags.value("CM"),             0, &AOClient::cmdLock          }},
        {"area_lock",     {ACLFlags.value("CM"),             0, &AOClient::cmdLock          }},
        {"spectatable",   {ACLFlags.value("CM"),             0, &AOClient::cmdSpectatable   }},
        {"area_spectate", {ACLFlags.value("CM"),             0, &AOClient::cmdSpectatable   }},
        {"unlock",        {ACLFlags.value("CM"),             0, &AOClient::cmdUnLock        }},
        {"area_unlock",   {ACLFlags.value("CM"),             0, &AOClient::cmdUnLock        }},
        {"timer",         {ACLFlags.value("CM"),             0, &AOClient::cmdTimer         }},
        {"area",          {ACLFlags.value("NONE"),           1, &AOClient::cmdArea          }},
        {"play",          {ACLFlags.value("CM"),             1, &AOClient::cmdPlay          }},
        {"areakick",      {ACLFlags.value("CM"),             1, &AOClient::cmdAreaKick      }},
        {"area_kick",     {ACLFlags.value("CM"),             1, &AOClient::cmdAreaKick      }},
        {"randomchar",    {ACLFlags.value("NONE"),           0, &AOClient::cmdRandomChar    }},
        {"switch",        {ACLFlags.value("NONE"),           1, &AOClient::cmdSwitch        }},
        {"toggleglobal",  {ACLFlags.value("NONE"),           0, &AOClient::cmdToggleGlobal  }},
        {"mods",          {ACLFlags.value("NONE"),           0, &AOClient::cmdMods          }},
        {"help",          {ACLFlags.value("NONE"),           0, &AOClient::cmdHelp          }},
        {"status",        {ACLFlags.value("NONE"),           1, &AOClient::cmdStatus        }},
        {"forcepos",      {ACLFlags.value("CM"),             2, &AOClient::cmdForcePos      }},
        {"currentmusic",  {ACLFlags.value("NONE"),           0, &AOClient::cmdCurrentMusic  }},
        {"pm",            {ACLFlags.value("NONE"),           2, &AOClient::cmdPM            }},
        {"evidence_mod",  {ACLFlags.value("CHANGE_EVI_MOD"), 1, &AOClient::cmdEvidenceMod   }},
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
