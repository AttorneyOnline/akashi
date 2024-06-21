#ifndef PACKET_RC_H
#define PACKET_RC_H

#include "network/aopacket.h"

class PacketRC : public AOPacket
{
  public:
    PacketRC(QStringList &contents);
    virtual PacketInfo getPacketInfo() const;
    virtual void handlePacket(AreaData *area, AOClient &client) const;
};
#endif
