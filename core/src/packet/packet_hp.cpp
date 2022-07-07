#include "include/packet/packet_hp.h"
#include "include/akashiutils.h"
#include "include/packet/packet_factory.h"
#include "include/server.h"

#include <QDebug>

PacketHP::PacketHP(QStringList &contents) :
    AOPacket(contents)
{
}

PacketInfo PacketHP::getPacketInfo() const
{
    PacketInfo info{
        .acl_permission = ACLRole::Permission::NONE,
        .min_args = 2,
        .header = "HP"};
    return info;
}

void PacketHP::handlePacket(AreaData *area, AOClient &client) const
{
    if (client.m_is_wtce_blocked) {
        client.sendServerMessage("You are blocked from using the judge controls.");
        return;
    }
    int l_newValue = m_content.at(1).toInt();

    if (m_content[0] == "1") {
        area->changeHP(AreaData::Side::DEFENCE, l_newValue);
    }
    else if (m_content[0] == "2") {
        area->changeHP(AreaData::Side::PROSECUTOR, l_newValue);
    }

    client.getServer()->broadcast(PacketFactory::createPacket("HP", {"1", QString::number(area->defHP())}), area->index());
    client.getServer()->broadcast(PacketFactory::createPacket("HP", {"2", QString::number(area->proHP())}), area->index());

    client.updateJudgeLog(area, &client, "updated the penalties");
}

bool PacketHP::validatePacket() const
{
    if (!AkashiUtils::checkArgType<int>(m_content.at(0)))
        return false;
    if (!AkashiUtils::checkArgType<int>(m_content.at(1)))
        return false;
    return true;
}
