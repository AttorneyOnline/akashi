#pragma once

#include "akashi_protocol_global.h"
#include "packet.h"

/*****************************************************************************
 * NETWORK PACKET DEFINITIONS
 * 
 * ClientPackets: Sent by the client to the server
 * ServerPackets: Sent by the server to the client
 * 
 * Yes, I'm aware this naming convention might not align with everyone's
 * preferences. I don't care. This is how it's organized here.
 *****************************************************************************/

namespace ServerPacket {
class AKASHI_PROTOCOL_EXPORT MS_V26 : public Packet
{
public:
    explicit MS_V26(const PacketData &f_data);
};

class AKASHI_PROTOCOL_EXPORT MS_V28 : public Packet
{};

} // namespace ServerPacket

namespace ClientPacket {
class AKASHI_PROTOCOL_EXPORT MS_V26 : public Packet
{
public:
    explicit MS_V26(const PacketData &f_data);
};

} // namespace ClientPacket
