#include "include/packet/packet_rm.h"
#include "include/server.h"

#include <QDebug>

PacketRM::PacketRM(QStringList &contents) :
    AOPacket(contents)
{
}

PacketInfo PacketRM::getPacketInfo() const
{
    PacketInfo info{
        .acl_permission = ACLRole::Permission::NONE,
        .min_args = 0,
        .header = "RM"};
    return info;
}

void PacketRM::handlePacket(AreaData *area, AOClient &client) const
{
    Q_UNUSED(area)

    client.sendPacket("SM", client.getServer()->getAreaNames() + client.getServer()->getMusicList());
}

bool PacketRM::validatePacket() const
{
    return true;
}
