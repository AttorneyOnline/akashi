#include "include/packet/packet_rc.h"
#include "include/server.h"

#include <QDebug>

PacketRC::PacketRC(QStringList &contents) :
    AOPacket(contents)
{
}

PacketInfo PacketRC::getPacketInfo() const
{
    PacketInfo info{
        .acl_permission = ACLRole::Permission::NONE,
        .min_args = 0,
        .header = "RC"};
    return info;
}

void PacketRC::handlePacket(AreaData *area, AOClient &client) const
{
    Q_UNUSED(area)

    client.sendPacket("SC", client.getServer()->getCharacters());
}

bool PacketRC::validatePacket() const
{
    return true;
}
