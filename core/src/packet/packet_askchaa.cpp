#include "include/packet/packet_askchaa.h"
#include "include/config_manager.h"
#include "include/server.h"

#include <QDebug>

PacketAskchaa::PacketAskchaa(QStringList &contents) :
    AOPacket(contents)
{
}

PacketInfo PacketAskchaa::getPacketInfo() const
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
    client.sendPacket("SI", {QString::number(client.getServer()->getCharacterCount()), "0", QString::number(client.getServer()->getAreaCount() + client.getServer()->getMusicList().length())});
}

bool PacketAskchaa::validatePacket() const
{
    if (m_content.size() > 0) { // Too many arguments.
        return false;
    }
    return true;
}
