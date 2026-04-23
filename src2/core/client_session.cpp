#include "core/client_session.h"

// How many outbound packets a session holds while its connection is down. Bounded
// so a session nobody deletes cannot hoard memory; a full buffer drops the
// oldest packet and records the overflow.
static const int PENDING_PACKET_LIMIT = 512;

namespace akashi {

ClientSession::ClientSession(int f_id, ITransport *f_transport, QObject *parent) :
    QObject(parent),
    id(f_id)
{
    bindTransport(f_transport);

    active_player = addPlayer(f_id, 1);

    afk_timer = new QTimer(this);
    afk_timer->setSingleShot(true);

    reconnect_timer = new QTimer(this);
    reconnect_timer->setSingleShot(true);
    connect(reconnect_timer, &QTimer::timeout, this, &ClientSession::reconnectTimedOut);
}

void ClientSession::waitForReconnect(int f_grace_seconds)
{
    waiting_for_reconnect = true;
    reconnect_timer->start(f_grace_seconds * 1000);
}

void ClientSession::cancelReconnectWait()
{
    waiting_for_reconnect = false;
    reconnect_timer->stop();
}

PlayerState *ClientSession::addPlayer(int f_id, int f_limit)
{
    if (players.size() >= f_limit) {
        return nullptr;
    }
    PlayerState *l_player = new PlayerState(f_id, this);
    players.append(l_player);
    return l_player;
}

void ClientSession::write(const Packet &f_packet)
{
    if (transport->isOpen()) {
        transport->write(f_packet);
        return;
    }

    if (pending_packets.size() >= PENDING_PACKET_LIMIT) {
        pending_packets.dequeue();
        pending_overflowed = true;
    }
    pending_packets.enqueue(f_packet);
}

void ClientSession::bindTransport(ITransport *f_transport)
{
    if (transport) {
        disconnect(transport, nullptr, this, nullptr);
        transport->deleteLater();
    }
    if (waiting_for_reconnect) {
        cancelReconnectWait();
    }

    transport = f_transport;
    transport->setParent(this);
    remote_ip = transport->peerAddress();

    // The capabilities the client announced while connecting, the header
    // side of the FL exchange. In the profile before ID resolves codecs,
    // so codec rules can key on them.
    const QStringList l_announced = transport->connectTimeFeatures();
    for (const QString &l_feature : l_announced) {
        profile.features.insert(l_feature);
    }
    connect(transport, &ITransport::packetReceived, this, &ClientSession::packetReceived);
    connect(transport, &ITransport::clientDisconnected, this, &ClientSession::transportClosed);

    while (!pending_packets.isEmpty() && transport->isOpen()) {
        transport->write(pending_packets.dequeue());
    }
}

} // namespace akashi
