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
#include "include/network/network_socket.h"
#include "include/packet/packet_factory.h"

NetworkSocket::NetworkSocket(QTcpSocket *f_socket, QObject *parent) :
    QObject(parent)
{
    m_socket_type = TCP;
    m_client_socket.tcp = f_socket;
    connect(m_client_socket.tcp, &QTcpSocket::readyRead,
            this, &NetworkSocket::readData);
    connect(m_client_socket.tcp, &QTcpSocket::disconnected,
            this, &NetworkSocket::clientDisconnected);
    m_socket_ip = m_client_socket.tcp->peerAddress();
}

NetworkSocket::NetworkSocket(QWebSocket *f_socket, QObject *parent) :
    QObject(parent)
{
    m_socket_type = WS;
    m_client_socket.ws = f_socket;
    connect(m_client_socket.ws, &QWebSocket::textMessageReceived,
            this, &NetworkSocket::ws_readData);
    connect(m_client_socket.ws, &QWebSocket::disconnected,
            this, &NetworkSocket::clientDisconnected);

    bool l_is_local = (m_client_socket.ws->peerAddress() == QHostAddress::LocalHost) ||
                      (m_client_socket.ws->peerAddress() == QHostAddress::LocalHostIPv6) ||
                      (m_client_socket.ws->peerAddress() == QHostAddress("::ffff:127.0.0.1"));
    // TLDR : We check if the header comes trough a proxy/tunnel running locally.
    // This is to ensure nobody can send those headers from the web.
    QNetworkRequest l_request = m_client_socket.ws->request();
    if (l_request.hasRawHeader("x-forwarded-for") && l_is_local) {
        m_socket_ip = QHostAddress(QString::fromUtf8(l_request.rawHeader("x-forwarded-for")));
    }
    else {
        m_socket_ip = f_socket->peerAddress();
    }
}

NetworkSocket::~NetworkSocket()
{
    if (m_socket_type == TCP) {
        m_client_socket.tcp->deleteLater();
    }
    else {
        m_client_socket.ws->deleteLater();
    }
}

QHostAddress NetworkSocket::peerAddress()
{
    return m_socket_ip;
}

void NetworkSocket::close()
{
    if (m_socket_type == TCP) {
        m_client_socket.tcp->deleteLater();
    }
    else {
        m_client_socket.ws->deleteLater();
    }
}

void NetworkSocket::close(QWebSocketProtocol::CloseCode f_code)
{
    m_client_socket.ws->close(f_code);
}

void NetworkSocket::readData()
{
    if (m_client_socket.tcp->bytesAvailable() > 30720) { // Client can send a max of 30KB to the server.
        m_client_socket.tcp->close();
    }

    QString l_data = QString::fromUtf8(m_client_socket.tcp->readAll());

    if (m_is_partial) {
        l_data = m_partial_packet + l_data;
    }
    if (!l_data.endsWith("%")) {
        m_is_partial = true;
    }

    QStringList l_all_packets = l_data.split("%");
    l_all_packets.removeLast(); // Remove the entry after the last delimiter

    if (l_all_packets.value(0).startsWith("MC", Qt::CaseInsensitive)) {
        l_all_packets = QStringList{l_all_packets.value(0)};
    }

    for (const QString &l_single_packet : qAsConst(l_all_packets)) {
        AOPacket* l_packet = PacketFactory::createPacket(l_single_packet);
        if (!l_packet) {
            qDebug() << "Unimplemented packet: " << l_single_packet;
            continue;
        }

        emit handlePacket(l_packet);
    }
}

void NetworkSocket::ws_readData(QString f_data)
{
    QString l_data = f_data;

    if (l_data.toUtf8().size() > 30720) {
        m_client_socket.ws->close(QWebSocketProtocol::CloseCodeTooMuchData);
    }

    QStringList l_all_packets = l_data.split("%");
    l_all_packets.removeLast(); // Remove the entry after the last delimiter

    if (l_all_packets.value(0).startsWith("MC", Qt::CaseInsensitive)) {
        l_all_packets = QStringList{l_all_packets.value(0)};
    }

    for (const QString &l_single_packet : qAsConst(l_all_packets)) {
        AOPacket* l_packet = PacketFactory::createPacket(l_single_packet);
        if (!l_packet) {
            qDebug() << "Unimplemented packet: " << l_single_packet;
            continue;
        }

        emit handlePacket(l_packet);
    }
}

void NetworkSocket::write(AOPacket *f_packet)
{
    if (m_socket_type == TCP) {
        m_client_socket.tcp->write(f_packet->toUtf8());
        m_client_socket.tcp->flush();
    }
    else {
        m_client_socket.ws->sendTextMessage(f_packet->toString());
        m_client_socket.ws->flush();
    }
}
