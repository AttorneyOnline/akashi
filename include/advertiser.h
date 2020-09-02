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
#ifndef MASTER_H
#define MASTER_H

#include "include/aopacket.h"

#include <QCoreApplication>
#include <QHostAddress>
#include <QString>
#include <QTcpSocket>

class Advertiser : public QObject {
    Q_OBJECT

  public:
    Advertiser(QString p_ip, int p_port, int p_ws_port, int p_local_port,
               QString p_name, QString p_description,
               QObject* parent = nullptr);
    ~Advertiser();
    void contactMasterServer();

signals:

  public slots:
    void readData();
    void socketConnected();
    void socketDisconnected();

  private:
    QString ip;
    int port;
    int ws_port;
    int local_port;
    QString name;
    QString description;

    QTcpSocket* socket;
};

#endif // MASTER_H
