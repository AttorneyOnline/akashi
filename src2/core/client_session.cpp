#include "core/client_session.h"

// How many outbound packets a session holds while its wire is down. Bounded
// so a session nobody reaps cannot hoard memory; a full buffer drops the
// oldest packet and records the overflow.
static const int PENDING_PACKET_LIMIT = 512;

namespace akashi {

ClientSession::ClientSession(int f_id, ITransport *f_transport, QObject *parent) :
    QObject(parent),
    id(f_id)
{
    bindTransport(f_transport);

    afk_timer = new QTimer(this);
    afk_timer->setSingleShot(true);
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
