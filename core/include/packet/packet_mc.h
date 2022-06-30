#ifndef PACKET_MC_H
#define PACKET_MC_H

#include "include/network/aopacket.h"

class PacketMC : public AOPacket
{
  public:
    PacketMC(QStringList &contents);
    virtual PacketInfo getPacketInfo() const;
    virtual void handlePacket(AreaData *area, AOClient &client) const;
    virtual bool validatePacket() const;
};
#endif
