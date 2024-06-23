#ifndef PACKET_RD_H
#define PACKET_RD_H

#include "network/aopacket.h"

class PacketRD : public AOPacket
{
  public:
    PacketRD(QStringList &contents);
    virtual PacketInfo getPacketInfo() const;
    virtual void handlePacket(AreaData *area, AOClient &client) const;
};
#endif
