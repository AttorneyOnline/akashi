#include "include/packet/packet_rt.h"
#include "include/packet/packet_factory.h"
#include "include/server.h"

#include <QDebug>

PacketRT::PacketRT(QStringList &contents) :
    AOPacket(contents)
{
}

PacketInfo PacketRT::getPacketInfo() const
{
    PacketInfo info{
        .acl_permission = ACLRole::Permission::NONE,
        .min_args = 1,
        .header = "RT"};
    return info;
}

void PacketRT::handlePacket(AreaData *area, AOClient &client) const
{
    if (client.m_is_wtce_blocked) {
        client.sendServerMessage("You are blocked from using the judge controls.");
        return;
    }

    if (!area->isWtceAllowed()) {
        client.sendServerMessage("WTCE animations have been disabled in this area.");
        return;
    }

    if (QDateTime::currentDateTime().toSecsSinceEpoch() - client.m_last_wtce_time <= 5)
        return;
    client.m_last_wtce_time = QDateTime::currentDateTime().toSecsSinceEpoch();
    client.getServer()->broadcast(PacketFactory::createPacket("RT", m_content), client.m_current_area);
    client.updateJudgeLog(area, &client, "WT/CE");
}

bool PacketRT::validatePacket() const
{
    return true;
}
