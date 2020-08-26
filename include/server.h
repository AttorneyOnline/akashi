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
#ifndef SERVER_H
#define SERVER_H

#include "include/aoclient.h"
#include "include/aopacket.h"
#include "include/area_data.h"
#include "include/ws_proxy.h"

#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QMap>
#include <QSettings>
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>

class AOClient;

class Server : public QObject {
    Q_OBJECT

  public:
    Server(int p_port, int p_ws_port, QObject* parent = nullptr);
    void start();
    AOClient* getClient(QString ipid);
    void updateCharsTaken(AreaData* area);
    void broadcast(AOPacket packet);

    int player_count;
    QStringList characters;
    QVector<AreaData*> areas;

  signals:

  public slots:
    void clientConnected();

  private:
    WSProxy* proxy;
    QTcpServer* server;

    int port;
    int ws_port;

    QVector<AOClient*> clients;
};

#endif // SERVER_H
