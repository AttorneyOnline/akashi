#ifndef PACKET_GENERIC_H
#define PACKET_GENERIC_H

#include "network/aopacket.h"

class PacketGeneric : public AOPacket
{
  public:
    PacketGeneric(QString header, QStringList contents);
    virtual PacketInfo getPacketInfo() const;
    virtual void handlePacket(AreaData *area, AOClient &client) const;

  private:
    QString header;
};
#endif
