#ifndef PACKET_EE_H
#define PACKET_EE_H

#include "network/aopacket.h"

class PacketEE : public AOPacket
{
  public:
    PacketEE(QStringList &contents);
    virtual PacketInfo getPacketInfo() const;
    virtual void handlePacket(AreaData *area, AOClient &client) const;
};
#endif
