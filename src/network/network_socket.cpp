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
#include "network/network_socket.h"
#include "packet/packet_factory.h"

NetworkSocket::NetworkSocket(QWebSocket *f_socket, QObject *parent) :
    QObject(parent)
{
    m_client_socket = f_socket;
    connect(m_client_socket, &QWebSocket::textMessageReceived, this, &NetworkSocket::handleMessage);
    connect(m_client_socket, &QWebSocket::disconnected, this, &NetworkSocket::clientDisconnected);

    bool l_is_local = (m_client_socket->peerAddress() == QHostAddress::LocalHost) ||
                      (m_client_socket->peerAddress() == QHostAddress::LocalHostIPv6) ||
                      (m_client_socket->peerAddress() == QHostAddress("::ffff:127.0.0.1"));
    // TLDR : We check if the header comes trough a proxy/tunnel running locally.
    // This is to ensure nobody can send those headers from the web.
    QNetworkRequest l_request = m_client_socket->request();
    if (l_request.hasRawHeader("x-forwarded-for") && l_is_local) {
        m_socket_ip = QHostAddress(QString::fromUtf8(l_request.rawHeader("x-forwarded-for")));
    }
    else {
        m_socket_ip = f_socket->peerAddress();
    }
}

NetworkSocket::~NetworkSocket()
{
    m_client_socket->deleteLater();
}

QHostAddress NetworkSocket::peerAddress()
{
    return m_socket_ip;
}

void NetworkSocket::close(QWebSocketProtocol::CloseCode f_code)
{
    m_client_socket->close(f_code);
}

void NetworkSocket::handleMessage(QString f_data)
{
    QString l_data = f_data;

    if (l_data.toUtf8().size() > 30720) {
        m_client_socket->close(QWebSocketProtocol::CloseCodeTooMuchData);
    }

    QStringList l_all_packets = l_data.split("%");
    l_all_packets.removeLast();  // Remove the entry after the last delimiter
    l_all_packets.removeAll({}); // Remove empty or null strings.

    if (l_all_packets.value(0).startsWith("MC", Qt::CaseInsensitive)) {
        l_all_packets = QStringList{l_all_packets.value(0)};
    }

    for (const QString &l_single_packet : qAsConst(l_all_packets)) {
        AOPacket *l_packet = PacketFactory::createPacket(l_single_packet);
        if (!l_packet) {
            qDebug() << "Unimplemented packet: " << l_single_packet;
            continue;
        }

        emit handlePacket(l_packet);
    }
}

void NetworkSocket::write(AOPacket *f_packet)
{
    m_client_socket->sendTextMessage(f_packet->toString());
}
