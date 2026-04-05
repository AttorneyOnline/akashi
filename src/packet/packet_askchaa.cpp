#include "packet/packet_askchaa.h"
#include "config_manager.h"
#include "server.h"

#include <QDebug>

PacketAskchaa::PacketAskchaa(QStringList &contents) :
    AOPacket(contents)
{
}

PacketInfo PacketAskchaa::packetInfo() const
{
    PacketInfo info{
        .acl_permission = ACLRole::Permission::NONE,
        .min_args = 0,
        .header = "askchaa"};
    return info;
}

void PacketAskchaa::handlePacket(AreaData *area, AOClient &client) const
{
    Q_UNUSED(area)
    // Evidence isn't loaded during this part anymore
    // As a result, we can always send "0" for evidence length
    // Client only cares about what it gets from LE
    client.sendPacket("SI", {QString::number(client.server()->characterCount()), "0", QString::number(client.server()->areaCount() + client.server()->musicList().length())});
}
