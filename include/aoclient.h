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

class AOClient : public QObject {
    Q_OBJECT
  public:
    AOClient(Server* p_server, QTcpSocket* p_socket, QObject* parent = nullptr, int user_id = 0);
    ~AOClient();

    Server* getServer();
    QString getIpid();
    void calculateIpid();

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
    bool is_muted = false;
    struct ClientVersion {
      QString string;
      int release = -1;
      int major = -1;
      int minor = -1;
    };
    ClientVersion version;

    QMap<QString, unsigned long long> ACLFlags {
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
        {"SUPER", ~0ULL}
    };

    bool is_shaken;
    bool is_disemvoweled;
    bool is_gimped;

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
    QString overlay = "";

    struct PacketInfo {
        unsigned long long acl_mask;
        int minArgs;
        void (AOClient::*action)(AreaData*, int, QStringList, AOPacket);
    };

    const QMap<QString, PacketInfo> packets {
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
        {"EE", {ACLFlags.value("NONE"), 4, &AOClient::pktEditEvidence}}
    };

    //// Commands
    void cmdDefault(int argc, QStringList argv);
    void cmdHelp(int argc, QStringList argv);
    void cmdMOTD(int argc, QStringList argv);
    void cmdAbout(int argc, QStringList argv);
    // Authentication
    void cmdLogin(int argc, QStringList argv);
    void cmdChangeAuth(int argc, QStringList argv);
    void cmdSetRootPass(int argc, QStringList argv);
    void cmdAddUser(int argc, QStringList argv);
    void cmdRemoveUser(int argc, QStringList argv);
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
    void cmdUnBan(int argc, QStringList argv);
    void cmdKick(int argc, QStringList argv);
    void cmdAnnounce(int argc, QStringList argv);
    void cmdM(int argc, QStringList argv);
    void cmdGM(int argc, QStringList argv);
    void cmdMute(int argc, QStringList argv);
    void cmdUnmute(int argc, QStringList argv);
    void cmdBans(int argc, QStringList argv);
    // Casing/RP
    void cmdPlay(int argc, QStringList argv);
    void cmdNeed(int argc, QStringList argv);
    void cmdFlip(int argc, QStringList argv);
    void cmdRoll(int argc, QStringList argv);
    void cmdRollP(int argc, QStringList argv);
    void cmdDoc(int argc, QStringList argv);
    void cmdClearDoc(int argc, QStringList argv);
    void cmdTimer(int argc, QStringList argv);
    void cmdEvidenceMod(int argc, QStringList argv);
    void cmdSubTheme(int argc, QStringList argv);
    // Messaging/Client
    void cmdPos(int argc, QStringList argv);
    void cmdForcePos(int argc, QStringList argv);
    void cmdSwitch(int argc, QStringList argv);
    void cmdRandomChar(int argc, QStringList argv);
    void cmdG(int argc, QStringList argv);
    void cmdToggleGlobal(int argc, QStringList argv);
    void cmdPM(int argc, QStringList argv);

    // Command helper functions
    QString getAreaTimer(int area_idx, QTimer* timer);
    QStringList buildAreaList(int area_idx);
    int genRand(int min, int max);
    void diceThrower(int argc, QStringList argv, RollType Type);
    long long parseTime(QString input);

    // Command function global variables
    bool change_auth_started = false;

    struct CommandInfo {
        unsigned long long acl_mask;
        int minArgs;
        void (AOClient::*action)(int, QStringList);
    };

    const QMap<QString, CommandInfo> commands {
        {"login", {ACLFlags.value("NONE"), 1, &AOClient::cmdLogin}},
        {"getareas", {ACLFlags.value("NONE"), 0 , &AOClient::cmdGetAreas}},
        {"getarea", {ACLFlags.value("NONE"), 0, &AOClient::cmdGetArea}},
        {"ban", {ACLFlags.value("BAN"), 2, &AOClient::cmdBan}},
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
        {"help", {ACLFlags.value("NONE"), 0, &AOClient::cmdHelp}},
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
        {"unmute", {ACLFlags.value("MUTE"), 1, &AOClient::cmdUnmute}},
        {"bans", {ACLFlags.value("BAN"), 0, &AOClient::cmdBans}},
        {"unban", {ACLFlags.value("BAN"), 1, &AOClient::cmdUnBan}},
        {"removeuser", {ACLFlags.value("MODIFY_USERS"), 1, &AOClient::cmdRemoveUser}},
        {"subtheme", {ACLFlags.value("CM"), 1, &AOClient::cmdSubTheme}},
        {"about", {ACLFlags.value("NONE"), 0, &AOClient::cmdAbout}}
    };

    QString partial_packet;
    bool is_partial;

    QString hwid;
    QString ipid;
    long last_wtce_time;
    QString last_message;
};

#endif // AOCLIENT_H
