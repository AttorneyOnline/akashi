//////////////////////////////////////////////////////////////////////////////////////
//    akashi - a server for Attorney Online 2                                       //
//    Copyright (C) 2020  scatterflower                                           //
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
#ifndef WS_CLIENT_H
#define WS_CLIENT_H

#include <QObject>
#include <QtWebSockets/QtWebSockets>
#include <QTcpSocket>
#include <QString>

class WSClient : public QObject {
    Q_OBJECT
public:
    WSClient(QTcpSocket* p_tcp_socket, QWebSocket* p_web_socket, QObject* parent = nullptr);
    ~WSClient();
public slots:
    void onTcpData();
    void onWsData(QString message);
    void onWsDisconnect();
    void onTcpDisconnect();


private:
    QTcpSocket* tcp_socket;
    QWebSocket* web_socket;
};

#endif // WS_CLIENT_H
