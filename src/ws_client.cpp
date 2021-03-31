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
    // Workaround for WebAO bug needing every packet in its own message
    QStringList all_packets = QString::fromUtf8(tcp_message).split("%");
    all_packets.removeLast(); // Remove empty space after final delimiter
    for(QString packet : all_packets) {
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
    tcp_socket->write(QString("WSIP#" + web_socket->peerAddress().toString() + "#%").toUtf8());
    tcp_socket->flush();
}

WSClient::~WSClient()
{
    tcp_socket->deleteLater();
    web_socket->deleteLater();
}
