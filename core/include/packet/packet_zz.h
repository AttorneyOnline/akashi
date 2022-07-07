#ifndef PACKET_ZZ_H
#define PACKET_ZZ_H

#include "include/network/aopacket.h"

class PacketZZ : public AOPacket
{
  public:
    PacketZZ(QStringList &contents);
    virtual PacketInfo getPacketInfo() const;
    virtual void handlePacket(AreaData *area, AOClient &client) const;
    virtual bool validatePacket() const;
};
#endif
