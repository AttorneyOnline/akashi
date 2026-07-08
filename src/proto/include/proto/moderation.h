#pragma once

#include "akashi_core_export.h"

namespace akashi {

class PacketRegistry;
class PacketCodecRegistry;

// Registers the handlers and codecs for the moderation packet family:
// ZZ modcalls and MA kick/ban actions from the mod call dialog.
AKASHI_CORE_EXPORT void registerModerationPackets(PacketRegistry &f_handlers, PacketCodecRegistry &f_codecs);

} // namespace akashi
