#include "packet.h"

#include "akashi_protocol_global.h"

namespace ServerPacket {
class AKASHI_PROTOCOL_EXPORT MS_V26 : public Packet
{
public:
    explicit MS_V26(const PacketData &f_data);
};
} // namespace ServerPacket

namespace ClientPacket {
class AKASHI_PROTOCOL_EXPORT MS_V26 : public Packet
{
public:
    explicit MS_V26(const PacketData &f_data);
};
} // namespace ClientPacket
