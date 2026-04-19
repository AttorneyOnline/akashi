#ifndef CORE_TRANSPORT_H
#define CORE_TRANSPORT_H

#include "akashi_core_export.h"
#include "proto/packet.h"

#include <QHostAddress>
#include <QObject>

namespace akashi {

// A client connection's transport, abstracted from the concrete protocol.
// Today the only implementation is WebSocket; native WSS, a legacy-TCP plugin,
// or a plugin's own framing are all first-class through this interface.
// ClientSession owns one by interface, so the protocol stays swappable, the
// session is testable with a fake transport, and the SDK never sees QWebSocket.
class AKASHI_CORE_EXPORT ITransport : public QObject
{
    Q_OBJECT

  public:
    // What a transport can do beyond moving packets. Features query these and
    // degrade when a capability is absent - e.g. a raw-TCP transport carries no
    // ConnectTimeMetadata, so connect-time dialect/version negotiation is simply
    // unavailable there and the client falls back to the in-band ID handshake.
    enum Capability
    {
        NoCapabilities = 0,
        ConnectTimeMetadata = 1 << 0, // carries handshake-time metadata (e.g. WS upgrade headers)
    };
    Q_DECLARE_FLAGS(Capabilities, Capability)

    explicit ITransport(QObject *parent = nullptr) :
        QObject(parent)
    {
    }
    ~ITransport() override = default;

    // The remote address, resolved through any trusted proxy the transport knows.
    virtual QHostAddress peerAddress() const = 0;

    // Sends one packet to the client.
    virtual void write(const Packet &f_packet) = 0;

    // Closes the connection.
    virtual void close() = 0;

    virtual Capabilities capabilities() const = 0;

    // The capability tokens the client announced while connecting. Tokens
    // in the network_ namespace arrive stripped - "network_auth_password"
    // reads as feature "auth_password", the same name FL speaks. Empty on
    // transports without ConnectTimeMetadata or when the client announced
    // nothing.
    virtual QStringList connectTimeFeatures() const { return {}; }

  Q_SIGNALS:
    // One packet parsed off the wire, including null packets from unreadable
    // data so the receiver can still rate-limit them.
    void packetReceived(const Packet &f_packet);

    // The connection has closed.
    void clientDisconnected();
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ITransport::Capabilities)

} // namespace akashi

#endif // CORE_TRANSPORT_H
