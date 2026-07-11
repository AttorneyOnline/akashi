#pragma once

#include "akashi_core_export.h"
#include "proto/packet.h"

#include <QHostAddress>
#include <QObject>

namespace akashi {

// How a connection ended. A clean end was chosen by somebody - the client
// quit or the server closed it - and the session ends with it. A lost
// connection dropped without a proper close (network problem, crash,
// timeout), which is the case where the session may stay alive for a while
// awaiting the person's return.
enum class DisconnectKind
{
    Clean,
    Lost,
};

// A client connection's transport, abstracted from the concrete protocol.
// Today the only implementation is WebSocket; native WSS, a legacy-TCP plugin,
// or a plugin's own framing are all first-class through this interface.
// ClientSession owns one by interface, so the protocol stays swappable, the
// session is testable with a fake transport, and the SDK never sees QWebSocket.
//
// Lifecycle contract every implementation must honour: clientDisconnected()
// is emitted EXACTLY ONCE, no matter how the connection ends - peer close,
// network loss, a local close() call, or a forced abort. The session's whole
// teardown hangs off that signal, so a transport that can lose it strands a
// phantom client forever. An implementation must therefore enforce liveness
// itself (close a dead or unresponsive peer) and must never wait unboundedly
// for a peer's cooperation to finish closing.
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

    // Sends one packet to the client. A no-op once the connection is down.
    virtual void write(const Packet &f_packet) = 0;

    // Closes the connection. clientDisconnected() still fires exactly once,
    // even against a peer that never completes the protocol's close exchange,
    // and no packetReceived is delivered from this point on.
    virtual void close() = 0;

    // False once the connection is down or closing. A connection can already
    // be dead on arrival - it died in the server's pending queue and emitted
    // its signals before anyone listened - and this is how callers detect it.
    virtual bool isOpen() const = 0;

    virtual Capabilities capabilities() const = 0;

    // The capability tokens the client announced while connecting. Tokens
    // in the network_ namespace arrive stripped - "network_auth_password"
    // reads as feature "auth_password", the same name FL speaks. Empty on
    // transports without ConnectTimeMetadata or when the client announced
    // nothing.
    virtual QStringList connectTimeFeatures() const { return {}; }

  Q_SIGNALS:
    // One packet parsed from incoming data, including null packets from unreadable
    // data so the receiver can still rate-limit them.
    void packetReceived(const akashi::Packet &f_packet);

    // The connection has closed. Emitted exactly once per connection.
    void clientDisconnected(akashi::DisconnectKind f_kind);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ITransport::Capabilities)

} // namespace akashi
