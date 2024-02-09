#ifndef PACKET_CT_H
#define PACKET_CT_H

#include "include/network/aopacket.h"

class PacketCT : public AOPacket
{
  public:
    PacketCT(QStringList &contents);
    virtual PacketInfo getPacketInfo() const;
    virtual void handlePacket(AreaData *area, AOClient &client) const;
    virtual bool validatePacket() const;
};
#endif
