#ifndef PACKET_ASKCHAA_H
#define PACKET_ASKCHAA_H

#include "network/aopacket.h"

class PacketAskchaa : public AOPacket
{
  public:
    PacketAskchaa(QStringList &contents);
    virtual PacketInfo getPacketInfo() const;
    virtual void handlePacket(AreaData *area, AOClient &client) const;
};
#endif
