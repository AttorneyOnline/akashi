#include "packet/packet_rc.h"
#include "server.h"

#include <QDebug>

PacketRC::PacketRC(QStringList &contents) :
    AOPacket(contents)
{
}

PacketInfo PacketRC::packetInfo() const
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

    client.sendPacket("SC", client.server()->characters());
}
