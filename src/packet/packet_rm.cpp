#include "packet/packet_rm.h"
#include "server.h"

#include <QDebug>

PacketRM::PacketRM(QStringList &contents) :
    AOPacket(contents)
{
}

PacketInfo PacketRM::packetInfo() const
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

    client.sendPacket("SM", client.server()->areaNames() + client.server()->musicList());
}
