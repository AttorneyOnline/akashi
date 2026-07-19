// AI-generated: written by Claude.
#pragma once

#include "proto/packet.h"
#include "proto/transport.h"

#include <QHostAddress>
#include <QList>

namespace akashi {

// A scriptable in-memory transport, standing in for a real socket.
class FakeTransport : public akashi::ITransport
{
  public:
    explicit FakeTransport(bool f_open = true, QObject *parent = nullptr) :
        akashi::ITransport(parent),
        m_open(f_open)
    {
    }

    QHostAddress peerAddress() const override { return m_peer; }
    bool isOpen() const override { return m_open; }
    Capabilities capabilities() const override { return NoCapabilities; }
    QStringList connectTimeFeatures() const override { return connect_features; }

    // Sets the peer address a client built on this transport reports; the
    // ClientSession reads it once at construction, so call this first.
    void setPeerAddress(const QHostAddress &f_peer) { m_peer = f_peer; }

    void write(const akashi::Packet &f_packet) override
    {
        if (m_open) {
            written.append(f_packet);
        }
    }

    void close() override
    {
        if (!m_open) {
            return;
        }
        m_open = false;
        Q_EMIT clientDisconnected(akashi::DisconnectKind::Clean);
    }

    // Simulates the connection dropping without a proper close.
    void loseConnection()
    {
        if (!m_open) {
            return;
        }
        m_open = false;
        Q_EMIT clientDisconnected(akashi::DisconnectKind::Lost);
    }

    QList<akashi::Packet> written;

    // What the fake claims the client announced while connecting.
    QStringList connect_features;

  private:
    bool m_open;
    QHostAddress m_peer = QHostAddress::LocalHost;
};

} // namespace akashi
