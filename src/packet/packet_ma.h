#pragma once

#include "network/aopacket.h"

class PacketMA : public AOPacket
{
  public:
    PacketMA(QStringList &contents);
    virtual PacketInfo packetInfo() const;
    virtual void handlePacket(AreaData *area, AOClient &client) const;
};
