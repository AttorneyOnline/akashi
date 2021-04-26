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
#include "include/ws_proxy.h"

WSProxy::WSProxy(int p_local_port, int p_ws_port, QObject* parent) :
    QObject(parent),
    local_port(p_local_port),
    ws_port(p_ws_port)
{
    server = new QWebSocketServer(QStringLiteral(""),
                                  QWebSocketServer::NonSecureMode, this);
    connect(server, &QWebSocketServer::newConnection, this,
            &WSProxy::wsConnected);
}

void WSProxy::start()
{
    if(!server->listen(QHostAddress::Any, ws_port)) {
        qDebug() << "WebSocket proxy failed to start: " << server->errorString();
    } else {
        qDebug() << "WebSocket proxy listening";
    }
}

void WSProxy::wsConnected()
{
    QWebSocket* new_ws = server->nextPendingConnection();
    QTcpSocket* new_tcp = new QTcpSocket(this);
    WSClient* client = new WSClient(new_tcp, new_ws, this);
    clients.append(client);

    connect(new_ws, &QWebSocket::textMessageReceived, client, &WSClient::onWsData);
    connect(new_tcp, &QTcpSocket::readyRead, client, &WSClient::onTcpData);
    connect(new_ws, &QWebSocket::disconnected, client, &WSClient::onWsDisconnect);
    connect(new_tcp, &QTcpSocket::disconnected, this, [=] {
        client->onTcpDisconnect();
        clients.removeAll(client);
        client->deleteLater();
    });
    connect(new_tcp, &QTcpSocket::connected, client, &WSClient::onTcpConnect);

    new_tcp->connectToHost(QHostAddress::LocalHost, local_port);
}

WSProxy::~WSProxy()
{
    server->deleteLater();
}
