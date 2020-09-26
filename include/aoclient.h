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

#include <algorithm>

#include <QHostAddress>
#include <QTcpSocket>
#include <QDateTime>

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

    struct CommandInfo {
        bool privileged;
        int minArgs;
    };

    const QMap<QString, CommandInfo> commands {
        {"login", {false, 1}},
        {"getareas", {false, 0 }},
        {"getarea", {false, 0}}
    };

    void handlePacket(AOPacket packet);
    void handleCommand(QString command, int argc, QStringList argv);
    void changeArea(int new_area);
    void arup(ARUPType type, bool broadcast);
    void fullArup();
    void sendServerMessage(QString message);

    QString partial_packet;
    bool is_partial;

    QString hwid;
    QString ipid;
    long last_wtce_time;
    QString last_message;

    bool authenticated = false;
    QString ooc_name = "";
};

#endif // AOCLIENT_H
