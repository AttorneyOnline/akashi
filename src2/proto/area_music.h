#ifndef PROTO_AREA_MUSIC_H
#define PROTO_AREA_MUSIC_H

#include "akashi_core_export.h"

namespace akashi {

class PacketRegistry;
class PacketCodecRegistry;

// Registers the handlers and codecs for the area and music packet family:
// MC, RT and PE.
AKASHI_CORE_EXPORT void registerAreaMusicPackets(PacketRegistry &f_handlers, PacketCodecRegistry &f_codecs);

} // namespace akashi

#endif // PROTO_AREA_MUSIC_H
