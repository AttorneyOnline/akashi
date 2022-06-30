#ifndef PACKET_GENERIC_H
#define PACKET_GENERIC_H

#include "include/network/aopacket.h"

class PacketGeneric : public AOPacket
{
  public:
    PacketGeneric(QString header, QStringList contents);
    virtual PacketInfo getPacketInfo() const;
    virtual void handlePacket(AreaData* area, AOClient& client) const;
    virtual bool validatePacket() const;
  private:
    QString header;
};
#endif