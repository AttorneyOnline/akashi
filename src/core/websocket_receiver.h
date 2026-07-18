#pragma once

#include "akashi/client_receiver.h"
#include "akashi_core_export.h"
#include "proto/packet.h"
#include "proto/transport.h"

#include <QHostAddress>
#include <QList>
#include <QNetworkRequest>
#include <QWebSocket>

#include <utility>

class QTimer;
class QWebSocketServer;

namespace akashi {

// A parsed proxy subnet: an address plus its prefix length, the shape
// QHostAddress::isInSubnet() takes. Empty means only localhost is trusted.
using TrustedProxyList = QList<std::pair<QHostAddress, int>>;

// The WebSocket door every AO2 client comes through: listens on one port
// and wraps each accepted socket in a WebSocketTransport.
class AKASHI_CORE_EXPORT WebSocketReceiver : public ClientReceiver
{
    Q_OBJECT

  public:
    // f_features is the same capability list the FL packet advertises
    // (proto::serverFeatures) - the one vocabulary, spoken here as
    // network_-prefixed subprotocol tokens. The upgrade response echoes
    // the FIRST OFFERED token the server speaks, so a client leads with
    // the capability it needs accepted - the one acceptance the handshake
    // may carry. New capabilities follow the [arch]_[packet]_[version]
    // grammar (network for generic ones), like ao_ms_2.11.1 or
    // network_auth_simple.
    WebSocketReceiver(const QHostAddress &f_address, int f_port,
                      const TrustedProxyList &f_trusted_proxies = {},
                      const QStringList &f_features = {}, QObject *parent = nullptr);

    bool start() override;
    QString lastError() const;
    int port() const;

  private:
    void onNewConnection();

    QWebSocketServer *m_server = nullptr;
    QHostAddress m_address;
    int m_port;
    TrustedProxyList m_trusted_proxies;
};

// The WebSocket transport: the one ITransport implementation core ships. Keeps
// the WebSocket framing and reverse-proxy IP resolution; the rest of the server
// only sees the akashi::ITransport interface.
//
// It enforces the ITransport lifecycle contract itself: the peer is pinged and
// aborted when it stops answering (catches half-open TCP and frozen clients),
// a close() that the peer never completes is aborted after a grace period, and
// clientDisconnected() is emitted exactly once whichever way the connection ends.
class AKASHI_CORE_EXPORT WebSocketTransport : public ITransport
{
    Q_OBJECT

  public:
    // Takes ownership of the QWebSocket.
    explicit WebSocketTransport(QWebSocket *f_socket, const TrustedProxyList &f_trusted_proxies = {},
                                QObject *parent = nullptr);

    QHostAddress peerAddress() const override;
    void write(const Packet &f_packet) override;
    void close() override;
    bool isOpen() const override;
    ITransport::Capabilities capabilities() const override;
    QStringList connectTimeFeatures() const override;

    /**
     * @brief Reads the client's announced capabilities out of the upgrade
     * request - the FL exchange's other direction, carried in the header.
     *
     * @details The subprotocol offer list is the client's feature list.
     * A network_-prefixed token becomes a feature under the same name FL
     * uses ("network_auth_password" reads as "auth_password"), and a
     * client-assembled packet key like "ao_ms_2.11.1" is kept whole. The
     * response may echo only one token (the first offered one the server
     * speaks), so the echo is an acceptance mark, not the exchange itself.
     * Static and pure so it can be tested directly.
     */
    static QStringList parseCapabilityTokens(const QNetworkRequest &f_request);

    /**
     * @brief Parses the trusted_proxies config string into subnets.
     *
     * @details Accepts comma-separated IPs or CIDR subnets; a bare IP is a
     * host route. Invalid entries are skipped with a warning. Static and pure
     * so it can be tested directly.
     */
    static TrustedProxyList parseTrustedProxies(const QString &f_csv);

    /**
     * @brief Whether a peer may set the client-IP proxy headers.
     *
     * @details localhost is always trusted; otherwise the peer must fall in
     * one of the configured trusted subnets. A direct internet client is not
     * trusted, so it cannot forge its apparent IP.
     */
    static bool isTrustedProxy(const QHostAddress &f_peer, const TrustedProxyList &f_trusted);

    /**
     * @brief Resolves the real client address behind a trusted proxy.
     *
     * @details Proxy headers are honoured only when the immediate peer is
     * trusted, so an untrusted client cannot forge them. For x-forwarded-for
     * the trusted proxy appends the peer it actually saw, so the LAST entry is
     * authoritative; earlier entries are client-supplied and spoofable. A
     * header that does not parse to a real address falls back to the socket
     * peer. Static and pure so it can be tested directly.
     */
    static QHostAddress resolveClientAddress(bool f_peer_trusted,
                                             const QNetworkRequest &f_request,
                                             const QHostAddress &f_peer);

  private Q_SLOTS:
    /**
     * @brief Splits incoming WebSocket data into packets.
     */
    void handleMessage(QString f_data);

    /**
     * @brief A pong came back, so the peer is alive.
     */
    void onPong();

    /**
     * @brief Classifies how the WebSocket ended and reports it.
     */
    void onSocketDisconnected();

    /**
     * @brief Pings the peer; aborts a peer that stopped answering.
     */
    void checkLiveness();

  private:
    /**
     * @brief Forwards the socket-level disconnect as exactly one clientDisconnected().
     */
    void reportDisconnect(DisconnectKind f_kind);

    /**
     * @brief Starts the WebSocket close exchange, aborting if the peer never finishes it.
     */
    void closeWithCode(QWebSocketProtocol::CloseCode f_code);

    /**
     * @brief Drops the connection immediately and reports the disconnect.
     */
    void abortConnection(DisconnectKind f_kind);

    QWebSocket *m_client_socket;

    /**
     * @brief Remote IP of the client.
     *
     * @details In the case of the WebSocket we also check if this has been proxy forwarded.
     */
    QHostAddress m_socket_ip;

    /**
     * @brief Guards the exactly-once clientDisconnected() emission.
     */
    bool m_disconnect_reported = false;

    /**
     * @brief Set once close() is called; inbound frames are dropped from then on.
     */
    bool m_closing = false;

    /**
     * @brief Pings sent since the last pong came back.
     */
    int m_unanswered_pings = 0;

    /**
     * @brief The capabilities the client announced in the upgrade request.
     */
    QStringList m_connect_features;

    QTimer *m_liveness_timer;
};

} // namespace akashi
