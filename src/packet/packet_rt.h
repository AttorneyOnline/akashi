#ifndef PACKET_RT_H
#define PACKET_RT_H

#include "network/aopacket.h"

class PacketRT : public AOPacket
{
  public:
    PacketRT(QStringList &contents);
    virtual PacketInfo getPacketInfo() const;
    virtual void handlePacket(AreaData *area, AOClient &client) const;
};
#endif
