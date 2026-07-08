#pragma once

#include "akashi_core_export.h"

namespace akashi {

class PacketRegistry;
class PacketCodecRegistry;

// Registers the handlers and codecs for the chat packet family:
// CT, DE, EE and SETCASE. MS follows once its message type exists.
AKASHI_CORE_EXPORT void registerChatPackets(PacketRegistry &f_handlers, PacketCodecRegistry &f_codecs);

} // namespace akashi
