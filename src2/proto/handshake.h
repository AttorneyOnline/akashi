#ifndef PROTO_HANDSHAKE_H
#define PROTO_HANDSHAKE_H

#include "akashi_core_export.h"

#include <QStringList>

namespace akashi {

class PacketRegistry;
class PacketCodecRegistry;

// Registers the handlers and codecs for the handshake packet family:
// HI, ID, askchaa, RC, RM, RD and CC.
AKASHI_CORE_EXPORT void registerHandshakePackets(PacketRegistry &f_handlers, PacketCodecRegistry &f_codecs);

// Every capability this server speaks; callers append the one auth token
// naming their active system. The single source for both advertising
// directions: the FL packet sends it as-is, and the connection handshake
// accepts the same names as network_-prefixed tokens.
AKASHI_CORE_EXPORT QStringList serverFeatures();

} // namespace akashi

#endif // PROTO_HANDSHAKE_H
