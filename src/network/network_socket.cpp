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

#include <QTimer>

// Ping cadence and how many may go unanswered before the peer counts as dead.
// Both Qt clients and browsers answer pings in their network stack, so a
// missed pong means a dead wire or a frozen client, not a busy one.
static const int PING_INTERVAL_MS = 30000;
static const int MAX_UNANSWERED_PINGS = 2;

// How long a started close exchange may wait for the peer before we abort.
static const int CLOSE_GRACE_MS = 10000;

NetworkSocket::NetworkSocket(QWebSocket *f_socket, QObject *parent) :
    akashi::ITransport(parent)
{
    m_client_socket = f_socket;
    m_client_socket->setParent(this);
    connect(m_client_socket, &QWebSocket::textMessageReceived, this, &NetworkSocket::handleMessage);
    connect(m_client_socket, &QWebSocket::disconnected, this, &NetworkSocket::reportDisconnect);
    connect(m_client_socket, &QWebSocket::pong, this, [this](quint64, const QByteArray &) {
        m_unanswered_pings = 0;
    });

    m_liveness_timer = new QTimer(this);
    connect(m_liveness_timer, &QTimer::timeout, this, &NetworkSocket::checkLiveness);
    m_liveness_timer->start(PING_INTERVAL_MS);

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
        // x-forwarded-for is a comma-separated list; the original client is the first entry.
        const QString l_forwarded = QString::fromUtf8(l_request.rawHeader("x-forwarded-for"));
        m_socket_ip = QHostAddress(l_forwarded.split(',').first().trimmed());
    }
    else {
        m_socket_ip = f_socket->peerAddress();
    }

    m_connect_features = parseCapabilityTokens(m_client_socket->request());
}

QStringList NetworkSocket::parseCapabilityTokens(const QNetworkRequest &f_request)
{
    QStringList l_features;
    const QString l_offered = QString::fromUtf8(f_request.rawHeader("Sec-WebSocket-Protocol"));
    const QStringList l_tokens = l_offered.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &l_token : l_tokens) {
        const QString l_trimmed = l_token.trimmed();
        // The network_ namespace carries FL feature names; a three-part
        // [arch]_[packet]_[version] key is kept whole. Anything else is
        // some other protocol's token and stays foreign.
        if (l_trimmed.startsWith(QStringLiteral("network_")) && l_trimmed.size() > 8) {
            l_features.append(l_trimmed.mid(8));
        }
        else if (l_trimmed.count(QLatin1Char('_')) >= 2) {
            l_features.append(l_trimmed);
        }
    }
    return l_features;
}

QStringList NetworkSocket::connectTimeFeatures() const
{
    return m_connect_features;
}

QHostAddress NetworkSocket::peerAddress() const
{
    return m_socket_ip;
}

void NetworkSocket::close()
{
    closeWithCode(QWebSocketProtocol::CloseCodeNormal);
}

bool NetworkSocket::isOpen() const
{
    return !m_closing && !m_disconnect_reported && m_client_socket->state() == QAbstractSocket::ConnectedState;
}

akashi::ITransport::Capabilities NetworkSocket::capabilities() const
{
    // The WebSocket upgrade carries handshake-time headers, so connect-time
    // negotiation is possible on this transport.
    return akashi::ITransport::ConnectTimeMetadata;
}

void NetworkSocket::closeWithCode(QWebSocketProtocol::CloseCode f_code)
{
    if (m_closing || m_disconnect_reported) {
        return;
    }
    m_closing = true;

    // A peer that never answers the close exchange must not hold the slot
    // open, so the wire is dropped once the grace period runs out.
    QTimer::singleShot(CLOSE_GRACE_MS, this, [this] {
        if (!m_disconnect_reported) {
            abortConnection();
        }
    });
    m_client_socket->close(f_code);
}

void NetworkSocket::abortConnection()
{
    m_client_socket->abort();
    reportDisconnect();
}

void NetworkSocket::reportDisconnect()
{
    if (m_disconnect_reported) {
        return;
    }
    m_disconnect_reported = true;
    m_liveness_timer->stop();
    Q_EMIT clientDisconnected();
}

void NetworkSocket::checkLiveness()
{
    if (m_unanswered_pings >= MAX_UNANSWERED_PINGS) {
        abortConnection();
        return;
    }
    ++m_unanswered_pings;
    m_client_socket->ping();
}

void NetworkSocket::handleMessage(QString f_data)
{
    // The connection is over once close() is called; frames racing in after
    // that (a rejected client still talking) must not reach the server.
    if (m_closing || m_disconnect_reported) {
        return;
    }

    QString l_data = f_data;

    if (l_data.toUtf8().size() > 30720) {
        closeWithCode(QWebSocketProtocol::CloseCodeTooMuchData);
        return;
    }

    // Split on the packet terminator, not on bare %, so an unescaped % in a
    // field only breaks its own packet instead of shredding the whole frame.
    QStringList l_all_packets = l_data.split("#%");
    l_all_packets.removeLast();  // Remove the entry after the last terminator
    l_all_packets.removeAll({}); // Remove empty or null strings.

    for (const QString &l_single_packet : qAsConst(l_all_packets)) {
        // Parsing wants the field separator back that the split consumed.
        Q_EMIT packetReceived(akashi::Packet::parse(l_single_packet + "#"));
    }
}

void NetworkSocket::write(const akashi::Packet &f_packet)
{
    if (!isOpen()) {
        return;
    }
    m_client_socket->sendTextMessage(f_packet.serialize());
}
