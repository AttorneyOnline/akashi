#ifndef PACKET_CASEA_H
#define PACKET_CASEA_H

#include "network/aopacket.h"

class PacketCasea : public AOPacket
{
  public:
    PacketCasea(QStringList &contents);
    virtual PacketInfo getPacketInfo() const;
    virtual void handlePacket(AreaData *area, AOClient &client) const;
};
#endif
