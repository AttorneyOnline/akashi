#ifndef AOCLIENT_H
#define AOCLIENT_H

#include "include/aopacket.h"
#include "include/server.h"

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
#include <QCryptographicHash>
#include <QHostAddress>
#include <QTcpSocket>

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

  private:
    Server* server;
    QTcpSocket* socket;

    void handlePacket(AOPacket packet);

    QString partial_packet;
    bool is_partial;

    QString hwid;
    QString ipid;
};

#endif // AOCLIENT_H
