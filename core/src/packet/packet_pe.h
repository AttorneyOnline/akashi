#ifndef PACKET_PE_H
#define PACKET_PE_H

#include "include/network/aopacket.h"

class PacketPE : public AOPacket
{
  public:
    PacketPE(QStringList &contents);
    virtual PacketInfo getPacketInfo() const;
    virtual void handlePacket(AreaData *area, AOClient &client) const;
    virtual bool validatePacket() const;
};
#endif
