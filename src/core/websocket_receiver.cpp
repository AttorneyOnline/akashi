#include "core/websocket_receiver.h"

#include <QTimer>
#include <QWebSocket>
#include <QWebSocketServer>

// Ping cadence and how many may go unanswered before the peer counts as dead.
// Both Qt clients and browsers answer pings in their network stack, so a
// missed pong means a dead connection or a frozen client, not a busy one.
static const int PING_INTERVAL_MS = 30000;
static const int MAX_UNANSWERED_PINGS = 2;

// How long a started close exchange may wait for the peer before we abort.
static const int CLOSE_GRACE_MS = 10000;

namespace akashi {

WebSocketReceiver::WebSocketReceiver(const QHostAddress &f_address, int f_port,
                                     const QStringList &f_features, QObject *parent) :
    ClientReceiver(parent),
    m_address(f_address),
    m_port(f_port)
{
    m_server = new QWebSocketServer(QStringLiteral("Akashi"), QWebSocketServer::NonSecureMode, this);

    // The FL capability list doubles as the subprotocol vocabulary. The
    // echo picks the client's first offered token found here, so the
    // client leads with what it needs accepted; offering only tokens the
    // server does not speak fails the handshake, which IS the refusal.
    QStringList l_spoken;
    for (const QString &l_feature : f_features) {
        l_spoken.append(QStringLiteral("network_") + l_feature);
    }
    m_server->setSupportedSubprotocols(l_spoken);
    connect(m_server, &QWebSocketServer::newConnection, this, &WebSocketReceiver::onNewConnection);
}

bool WebSocketReceiver::start()
{
    return m_server->listen(m_address, m_port);
}

QString WebSocketReceiver::lastError() const
{
    return m_server->errorString();
}

int WebSocketReceiver::port() const
{
    return m_server->serverPort();
}

void WebSocketReceiver::onNewConnection()
{
    QWebSocket *l_socket = m_server->nextPendingConnection();
    Q_EMIT inboundClient(new WebSocketTransport(l_socket));
}

WebSocketTransport::WebSocketTransport(QWebSocket *f_socket, QObject *parent) :
    ITransport(parent)
{
    m_client_socket = f_socket;
    m_client_socket->setParent(this);
    connect(m_client_socket, &QWebSocket::textMessageReceived, this, &WebSocketTransport::handleMessage);
    connect(m_client_socket, &QWebSocket::disconnected, this, &WebSocketTransport::onSocketDisconnected);
    connect(m_client_socket, &QWebSocket::pong, this, &WebSocketTransport::onPong);

    m_liveness_timer = new QTimer(this);
    connect(m_liveness_timer, &QTimer::timeout, this, &WebSocketTransport::checkLiveness);
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

QStringList WebSocketTransport::parseCapabilityTokens(const QNetworkRequest &f_request)
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

QStringList WebSocketTransport::connectTimeFeatures() const
{
    return m_connect_features;
}

QHostAddress WebSocketTransport::peerAddress() const
{
    return m_socket_ip;
}

void WebSocketTransport::close()
{
    closeWithCode(QWebSocketProtocol::CloseCodeNormal);
}

bool WebSocketTransport::isOpen() const
{
    return !m_closing && !m_disconnect_reported && m_client_socket->state() == QAbstractSocket::ConnectedState;
}

ITransport::Capabilities WebSocketTransport::capabilities() const
{
    // The WebSocket upgrade carries handshake-time headers, so connect-time
    // negotiation is possible on this transport.
    return ITransport::ConnectTimeMetadata;
}

void WebSocketTransport::closeWithCode(QWebSocketProtocol::CloseCode f_code)
{
    if (m_closing || m_disconnect_reported) {
        return;
    }
    m_closing = true;

    // A peer that never answers the close exchange must not hold the slot
    // open, so the connection is dropped once the grace period runs out. We chose
    // to close, so this still ends the connection cleanly.
    QTimer::singleShot(CLOSE_GRACE_MS, this, [this] {
        if (!m_disconnect_reported) {
            abortConnection(DisconnectKind::Clean);
        }
    });
    m_client_socket->close(f_code);
}

void WebSocketTransport::abortConnection(DisconnectKind f_kind)
{
    m_client_socket->abort();
    reportDisconnect(f_kind);
}

void WebSocketTransport::onSocketDisconnected()
{
    // No close frame means the connection just died; a proper close exchange - or a
    // close we started ourselves - is somebody finishing the connection.
    bool l_lost = !m_closing && m_client_socket->closeCode() == QWebSocketProtocol::CloseCodeAbnormalDisconnection;
    reportDisconnect(l_lost ? DisconnectKind::Lost : DisconnectKind::Clean);
}

void WebSocketTransport::reportDisconnect(DisconnectKind f_kind)
{
    if (m_disconnect_reported) {
        return;
    }
    m_disconnect_reported = true;
    m_liveness_timer->stop();
    Q_EMIT clientDisconnected(f_kind);
}

void WebSocketTransport::onPong()
{
    m_unanswered_pings = 0;
}

void WebSocketTransport::checkLiveness()
{
    if (m_unanswered_pings >= MAX_UNANSWERED_PINGS) {
        abortConnection(DisconnectKind::Lost);
        return;
    }
    ++m_unanswered_pings;
    m_client_socket->ping();
}

void WebSocketTransport::handleMessage(QString f_data)
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

    for (const QString &l_single_packet : std::as_const(l_all_packets)) {
        // Parsing wants the field separator back that the split consumed.
        Q_EMIT packetReceived(Packet::parse(l_single_packet + "#"));
    }
}

void WebSocketTransport::write(const Packet &f_packet)
{
    if (!isOpen()) {
        return;
    }
    m_client_socket->sendTextMessage(f_packet.serialize());
}

} // namespace akashi
