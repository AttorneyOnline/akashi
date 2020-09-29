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
#include "include/icchatpacket.h"
#include "include/area_data.h"
#include "include/db_manager.h"

#include <algorithm>

#include <QHostAddress>
#include <QTcpSocket>
#include <QDateTime>
#include <QRandomGenerator>

class Server;

class AOClient : public QObject {
    Q_OBJECT
  public:
    AOClient(Server* p_server, QTcpSocket* p_socket, QObject* parent = nullptr);
    ~AOClient();

    QString getHwid();
    void setHwid(QString p_hwid);

    QString getIpid();

    QHostAddress remote_ip;
    QString password;
    bool joined;
    int current_area;
    QString current_char;
    bool authenticated = false;
    QString moderator_name = "";
    QString ooc_name = "";

    enum ACLFlags {
        NONE = 0ULL,
        KICK = 1ULL << 0,
        BAN = 1ULL << 1,
        BGLOCK = 1ULL << 2,
        SUPER = ~0ULL
    };

  public slots:
    void clientDisconnected();
    void clientData();
    void sendPacket(AOPacket packet);
    void sendPacket(QString header, QStringList contents);
    void sendPacket(QString header);

  private:
    Server* server;
    QTcpSocket* socket;

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

    struct PacketInfo {
        unsigned long long acl_mask;
        int minArgs;
        void (AOClient::*action)(AreaData*, int, QStringList, AOPacket);
    };

    const QMap<QString, PacketInfo> packets {
        {"HI", {ACLFlags::NONE, 1, &AOClient::pktHardwareId}},
        {"ID", {ACLFlags::NONE, 2, &AOClient::pktSoftwareId}},
        {"askchaa", {ACLFlags::NONE, 0, &AOClient::pktBeginLoad}},
        {"RC", {ACLFlags::NONE, 0, &AOClient::pktRequestChars}},
        {"RM", {ACLFlags::NONE, 0, &AOClient::pktRequestMusic}},
        {"RD", {ACLFlags::NONE, 0, &AOClient::pktLoadingDone}},
        {"PW", {ACLFlags::NONE, 1, &AOClient::pktCharPassword}},
        {"CC", {ACLFlags::NONE, 3, &AOClient::pktSelectChar}},
        {"MS", {ACLFlags::NONE, 1, &AOClient::pktIcChat}}, // TODO: doublecheck
        {"CT", {ACLFlags::NONE, 2, &AOClient::pktOocChat}},
        {"CH", {ACLFlags::NONE, 1, &AOClient::pktPing}},
        {"MC", {ACLFlags::NONE, 2, &AOClient::pktChangeMusic}},
        {"RT", {ACLFlags::NONE, 1, &AOClient::pktWtCe}},
        {"HP", {ACLFlags::NONE, 2, &AOClient::pktHpBar}},
        {"WSIP", {ACLFlags::NONE, 1, &AOClient::pktWebSocketIp}}
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
        {"login", {ACLFlags::NONE, 1, &AOClient::cmdLogin}},
        {"getareas", {ACLFlags::NONE, 0 , &AOClient::cmdGetAreas}},
        {"getarea", {ACLFlags::NONE, 0, &AOClient::cmdGetArea}},
        {"ban", {ACLFlags::BAN, 2, &AOClient::cmdBan}},
        {"kick", {ACLFlags::KICK, 2, &AOClient::cmdKick}},
        {"changeauth", {ACLFlags::SUPER, 0, &AOClient::cmdChangeAuth}},
        {"rootpass", {ACLFlags::SUPER, 1, &AOClient::cmdSetRootPass}},
        {"background", {ACLFlags::NONE, 1, &AOClient::cmdSetBackground}},
        {"bg", {ACLFlags::NONE, 1, &AOClient::cmdSetBackground}},
        {"bglock", {ACLFlags::BGLOCK, 0, &AOClient::cmdBgLock}},
        {"bgunlock", {ACLFlags::BGLOCK, 0, &AOClient::cmdBgUnlock}}
    };

    QString partial_packet;
    bool is_partial;

    QString hwid;
    QString ipid;
    long last_wtce_time;
    QString last_message;
};

#endif // AOCLIENT_H
