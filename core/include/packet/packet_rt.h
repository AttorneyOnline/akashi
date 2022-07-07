#ifndef PACKET_RT_H
#define PACKET_RT_H

#include "include/network/aopacket.h"

class PacketRT : public AOPacket
{
  public:
    PacketRT(QStringList &contents);
    virtual PacketInfo getPacketInfo() const;
    virtual void handlePacket(AreaData *area, AOClient &client) const;
    virtual bool validatePacket() const;
};
#endif
