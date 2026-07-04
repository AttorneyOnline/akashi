#pragma once

#include "akashi_core_export.h"
#include "proto/transport.h"

#include <QObject>

namespace akashi {

// Accepts connections for one protocol and hands each one over as a
// transport. Core ships the WebSocket receiver; a plugin can subclass this
// to serve any custom connection protocol and connect its inboundClient
// signal to the server's inboundClient slot.
class AKASHI_CORE_EXPORT ClientReceiver : public QObject
{
    Q_OBJECT

  public:
    using QObject::QObject;

    // Starts accepting connections; false when the protocol's endpoint
    // could not be opened.
    virtual bool start() = 0;

  Q_SIGNALS:
    // A new connection arrived, already wrapped in its transport. The
    // receiver keeps no ownership: whoever handles the signal adopts it.
    void inboundClient(akashi::ITransport *f_transport);
};

} // namespace akashi
