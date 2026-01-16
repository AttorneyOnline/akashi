#include "packet/packet_hp.h"
#include "akashiutils.h"
#include "packet/packet_factory.h"
#include "server.h"

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
    if (client.m_is_spectator) {
        client.sendServerMessage("Spectators are blocked from using the judge controls.");
        return;
    }

    if (area->lockStatus() == AreaData::LockStatus::SPECTATABLE && !area->invited().contains(client.clientId()) && !client.checkPermission(ACLRole::BYPASS_LOCKS)) {
        client.sendServerMessage("Spectators are blocked from using the judge controls.");
        return;
    }
    
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
