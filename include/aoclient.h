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
#include <QtGlobal>
#if QT_VERSION > QT_VERSION_CHECK(5, 10, 0)
#include <QRandomGenerator>
#endif

class Server;

class AOClient : public QObject {
    Q_OBJECT
  public:
    AOClient(Server* p_server, QTcpSocket* p_socket, QObject* parent = nullptr);
    ~AOClient();

    QString getHwid();
    QString getIpid();
    Server* getServer();
    void setHwid(QString p_hwid);

    QHostAddress remote_ip;
    QString password;
    bool joined;
    int current_area;
    QString current_char;
    bool authenticated = false;
    QString moderator_name = "";
    QString ooc_name = "";

    QMap<QString, unsigned long long> ACLFlags {
        {"NONE", 0ULL},
        {"KICK", 1ULL << 0},
        {"BAN", 1ULL << 1},
        {"BGLOCK", 1ULL << 2},
        {"MODIFY_USERS", 1ULL << 3},
        {"SUPER", ~0ULL}
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

    void handlePacket(AOPacket packet);
    void handleCommand(QString command, int argc, QStringList argv);
    void changeArea(int new_area);
    void arup(ARUPType type, bool broadcast);
    void fullArup();
    void sendServerMessage(QString message);
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
    AOPacket validateIcPacket(AOPacket packet);

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

    // Commands
    void cmdDefault(int argc, QStringList argv);
    void cmdLogin(int argc, QStringList argv);
    void cmdGetAreas(int argc, QStringList argv);
    void cmdGetArea(int argc, QStringList argv);
    void cmdBan(int argc, QStringList argv);
    void cmdKick(int argc, QStringList argv);
    void cmdChangeAuth(int argc, QStringList argv);
    void cmdSetRootPass(int argc, QStringList argv);
    void cmdSetBackground(int argc, QStringList argv);
    void cmdBgLock(int argc, QStringList argv);
    void cmdBgUnlock(int argc, QStringList argv);
    void cmdAddUser(int argc, QStringList argv);
    void cmdListPerms(int argc, QStringList argv);
    void cmdAddPerms(int argc, QStringList argv);
    void cmdRemovePerms(int argc, QStringList argv);
    void cmdListUsers(int argc, QStringList argv);
    void cmdLogout(int argc, QStringList argv);
    void cmdPos(int argc, QStringList argv);
    void cmdG(int argc, QStringList argv);
    void cmdNeed(int argc, QStringList argv);

    // Command helper functions
    QStringList buildAreaList(int area_idx);

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
        {"need", {ACLFlags.value("NONE"), 1, &AOClient::cmdNeed}}
    };

    QString partial_packet;
    bool is_partial;

    QString hwid;
    QString ipid;
    long last_wtce_time;
    QString last_message;
};

#endif // AOCLIENT_H
