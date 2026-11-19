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
#include "config_manager.h"
#include "packet/packet_factory.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

NetworkSocket::NetworkSocket(QWebSocket *f_socket, QObject *parent) :
    QObject(parent),
    m_debug_enabled(ConfigManager::debugEnabled())
{
    m_client_socket = f_socket;
    connect(m_client_socket, &QWebSocket::textMessageReceived, this, &NetworkSocket::handleMessage);
    connect(m_client_socket, &QWebSocket::disconnected, this, [this] {
        logDisconnect(true);
        emit clientDisconnected();
    });

    if (m_debug_enabled) {
        m_connection_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_connection_duration.start();
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        connect(m_client_socket, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError f_error) {
#else
        connect(m_client_socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, [this](QAbstractSocket::SocketError f_error) {
#endif
            m_has_socket_error = true;
            m_socket_error = f_error;
            m_socket_error_string = m_client_socket->errorString();
        });
    }

    bool l_is_local = (m_client_socket->peerAddress() == QHostAddress::LocalHost) ||
                      (m_client_socket->peerAddress() == QHostAddress::LocalHostIPv6) ||
                      (m_client_socket->peerAddress() == QHostAddress("::ffff:127.0.0.1"));
    // TLDR : We check if the header comes trough a proxy/tunnel running locally.
    // This is to ensure nobody can send those headers from the web.
    QNetworkRequest l_request = m_client_socket->request();
    if (l_request.hasRawHeader("x-real-ip") && l_is_local) {
        m_socket_ip = QHostAddress(QString::fromUtf8(l_request.rawHeader("x-real-ip")));
    }
    else if (l_request.hasRawHeader("x-forwarded-for") && l_is_local) {
        m_socket_ip = QHostAddress(QString::fromUtf8(l_request.rawHeader("x-forwarded-for")));
    }
    else {
        m_socket_ip = f_socket->peerAddress();
    }
}

NetworkSocket::~NetworkSocket()
{
    logDisconnect(false);
    if (!m_client_socket.isNull()) {
        disconnect(m_client_socket, nullptr, this, nullptr);
    }
    m_client_socket = nullptr;
}

QHostAddress NetworkSocket::peerAddress()
{
    return m_socket_ip;
}

void NetworkSocket::close(const char *f_reason, QWebSocketProtocol::CloseCode f_code)
{
    if (m_debug_enabled && m_server_close_reason.isEmpty()) {
        m_server_close_reason = QString::fromLatin1(f_reason);
    }
    m_disconnecting = true;
    if (!m_client_socket.isNull()) {
        m_client_socket->close(f_code);
    }
}

void NetworkSocket::setClientInfo(int f_client_id, const QString &f_ipid)
{
    if (m_debug_enabled) {
        m_client_id = f_client_id;
        m_ipid = f_ipid;
    }
}

void NetworkSocket::setJoined()
{
    if (m_debug_enabled) {
        m_joined = true;
    }
}

void NetworkSocket::logServerShutdown()
{
    if (m_debug_enabled && m_server_close_reason.isEmpty()) {
        m_server_close_reason = QStringLiteral("server_shutdown");
    }
    logDisconnect(false);
}

void NetworkSocket::logDisconnect(bool f_disconnected)
{
    if (!m_debug_enabled || m_disconnect_logged) {
        return;
    }
    m_disconnect_logged = true;

    QString classification = QStringLiteral("unknown");
    if (!m_server_close_reason.isEmpty()) {
        classification = QStringLiteral("server_initiated");
    }
    else if (m_has_socket_error && m_socket_error != QAbstractSocket::RemoteHostClosedError) {
        classification = QStringLiteral("socket_error");
    }
    else if (f_disconnected && !m_client_socket.isNull() &&
             m_client_socket->closeCode() == QWebSocketProtocol::CloseCodeGoingAway) {
        classification = QStringLiteral("peer_going_away");
    }

    QJsonObject details{
        {"connection_id", m_connection_id},
        {"client_id", m_client_id},
        {"ipid", m_ipid},
        {"joined", m_joined},
        {"duration_ms", m_connection_duration.elapsed()},
        {"classification", classification},
        {"server_reason", m_server_close_reason},
        {"disconnected_signal", f_disconnected}};
    if (f_disconnected && !m_client_socket.isNull()) {
        details.insert("close_code", static_cast<int>(m_client_socket->closeCode()));
        details.insert("close_reason", m_client_socket->closeReason().left(123));
    }
    if (m_has_socket_error) {
        details.insert("socket_error", static_cast<int>(m_socket_error));
        details.insert("socket_error_string", m_socket_error_string);
    }
    qInfo().noquote() << "connection_end" << QJsonDocument(details).toJson(QJsonDocument::Compact);
}

void NetworkSocket::handleMessage(QString f_data)
{
    // Ignore messages if we're already disconnecting
    if (m_disconnecting) {
        return;
    }

    QString l_data = f_data;

    if (l_data.toUtf8().size() > 30720) {
        if (m_debug_enabled && m_server_close_reason.isEmpty()) {
            m_server_close_reason = QStringLiteral("message_too_large");
        }
        if (!m_client_socket.isNull())
            m_client_socket->close(QWebSocketProtocol::CloseCodeTooMuchData);
        return;
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

bool NetworkSocket::isAlive() const
{
    if (m_disconnecting) {
        return false;
    }

    if (m_client_socket.isNull()) {
        return false;
    }

    if (m_client_socket->state() != QAbstractSocket::ConnectedState) {
        return false;
    }

    return true;
}

void NetworkSocket::write(AOPacket *f_packet)
{
    if (!isAlive()) {
        return;
    }

    m_client_socket->sendTextMessage(f_packet->toString());
}
