#ifndef PACKET_RM_H
#define PACKET_RM_H

#include "network/aopacket.h"

class PacketRM : public AOPacket
{
  public:
    PacketRM(QStringList &contents);
    virtual PacketInfo getPacketInfo() const;
    virtual void handlePacket(AreaData *area, AOClient &client) const;
};
#endif
