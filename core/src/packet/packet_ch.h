#ifndef PACKET_CH_H
#define PACKET_CH_H

#include "aopacket.h"

class PacketCH : public AOPacket
{
  public:
    PacketCH(QStringList &contents);
    virtual PacketInfo getPacketInfo() const;
    virtual void handlePacket(AreaData *area, AOClient &client) const;
    virtual bool validatePacket() const;
};
#endif
