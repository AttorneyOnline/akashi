#ifndef PACKET_RM_H
#define PACKET_RM_H

#include "include/network/aopacket.h"

class PacketRM : public AOPacket
{
  public:
    PacketRM(QStringList &contents);
    virtual PacketInfo getPacketInfo() const;
    virtual void handlePacket(AreaData *area, AOClient &client) const;
    virtual bool validatePacket() const;
};
#endif
