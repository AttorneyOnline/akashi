#ifndef PROTO_IC_H
#define PROTO_IC_H

#include "akashi_core_export.h"
#include "proto/ic_message.h"
#include "proto/packet_codec.h"

namespace akashi {

class PacketRegistry;

// The positional MS packet. Decode reads the sender's field layout;
// encode writes the wider layout the server sends back to the room.
class AKASHI_CORE_EXPORT Ao2IcCodec : public Codec
{
  public:
    std::unique_ptr<Message> decode(const Packet &f_packet) const override;
    Packet encode(const Message &f_message) const override;
};

// Registers the MS handler and codec.
AKASHI_CORE_EXPORT void registerIcPackets(PacketRegistry &f_handlers, PacketCodecRegistry &f_codecs);

// Reads a message back out of the wider outgoing field layout, so a
// testimony-adjusted broadcast can be re-examined as a message.
AKASHI_CORE_EXPORT ICMessage icMessageFromOutgoingFields(const QStringList &f_fields);

} // namespace akashi

#endif // PROTO_IC_H
