#ifndef PACKET_ID_H
#define PACKET_ID_H

#include "network/aopacket.h"

class PacketID : public AOPacket
{
  public:
    PacketID(QStringList &contents);
    virtual PacketInfo getPacketInfo() const;
    virtual void handlePacket(AreaData *area, AOClient &client) const;
};
#endif
