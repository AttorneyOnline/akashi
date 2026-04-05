#ifndef PACKET_MS_H
#define PACKET_MS_H

#include "network/aopacket.h"
#include "proto/packet.h"

class PacketMS : public AOPacket
{
  public:
    PacketMS(QStringList &contents);
    virtual PacketInfo packetInfo() const;
    virtual void handlePacket(AreaData *area, AOClient &client) const;

  private:
    akashi::Packet validateIcPacket(AOClient &client) const;
    QRegularExpressionMatch isTestimonyJumpCommand(QString message) const;
};
#endif
