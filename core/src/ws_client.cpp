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
#include "include/ws_client.h"

void WSClient::onWsData(QString message)
{
    tcp_socket->write(message.toUtf8());
    tcp_socket->flush();
}

void WSClient::onTcpData()
{
    QByteArray tcp_message = tcp_socket->readAll();

    if (!tcp_message.endsWith("#%")) {
        partial_packet.append(tcp_message);
        is_segmented = true;
        return;
    }

    if (is_segmented) {
        partial_packet.append(tcp_message);
        tcp_message = partial_packet;
        partial_packet.clear();
        is_segmented = false;
    }

    // Workaround for WebAO bug needing every packet in its own message
    QStringList all_packets = QString::fromUtf8(tcp_message).split("%");
    all_packets.removeLast(); // Remove empty space after final delimiter
    for (const QString &packet : qAsConst(all_packets)) {
        web_socket->sendTextMessage(packet + "%");
    }
}

void WSClient::onWsDisconnect()
{
    if (tcp_socket != nullptr)
        tcp_socket->disconnectFromHost();
}

void WSClient::onTcpDisconnect()
{
    web_socket->close();
}

void WSClient::onTcpConnect()
{
    tcp_socket->write(QString("WSIP#" + websocket_ip + "#%").toUtf8());
    tcp_socket->flush();
}

WSClient::WSClient(QTcpSocket *p_tcp_socket, QWebSocket *p_web_socket, QObject *parent) :
    QObject(parent),
    tcp_socket(p_tcp_socket),
    web_socket(p_web_socket)
{
    bool l_is_local = (web_socket->peerAddress() == QHostAddress::LocalHost) ||
                      (web_socket->peerAddress() == QHostAddress::LocalHostIPv6);
    // TLDR : We check if the header comes trough a proxy/tunnel running locally.
    // This is to ensure nobody can send those headers from the web.
    QNetworkRequest l_request = web_socket->request();
    if (l_request.hasRawHeader("x-forwarded-for") && l_is_local) {
        websocket_ip = l_request.rawHeader("x-forwarded-for");
    }
    else {
        websocket_ip = web_socket->peerAddress().toString();
    }
}

WSClient::~WSClient()
{
    tcp_socket->deleteLater();
    web_socket->deleteLater();
}
