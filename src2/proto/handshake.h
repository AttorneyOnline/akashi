#ifndef PROTO_HANDSHAKE_H
#define PROTO_HANDSHAKE_H

#include "akashi_core_export.h"

namespace akashi {

class PacketRegistry;
class PacketCodecRegistry;

// Registers the handlers and codecs for the handshake packet family:
// HI, ID, askchaa, RC, RM, RD and CC.
AKASHI_CORE_EXPORT void registerHandshakePackets(PacketRegistry &f_handlers, PacketCodecRegistry &f_codecs);

} // namespace akashi

#endif // PROTO_HANDSHAKE_H
