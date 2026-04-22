// AI-generated: written by Claude.
#pragma once

#include "core/transport.h"
#include "proto/packet.h"

#include <QHostAddress>
#include <QList>

// A scriptable in-memory transport, standing in for a real socket.
class FakeTransport : public akashi::ITransport
{
  public:
    explicit FakeTransport(bool f_open = true, QObject *parent = nullptr) :
        akashi::ITransport(parent),
        m_open(f_open)
    {
    }

    QHostAddress peerAddress() const override { return QHostAddress::LocalHost; }
    bool isOpen() const override { return m_open; }
    Capabilities capabilities() const override { return NoCapabilities; }
    QStringList connectTimeFeatures() const override { return connect_features; }

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
        Q_EMIT clientDisconnected();
    }

    QList<akashi::Packet> written;

    // What the fake claims the client announced while connecting.
    QStringList connect_features;

  private:
    bool m_open;
};
