#include "include/packet/packet_de.h"
#include "include/akashiutils.h"
#include "include/server.h"

#include <QDebug>

PacketDE::PacketDE(QStringList &contents) :
    AOPacket(contents)
{
}

PacketInfo PacketDE::getPacketInfo() const
{
    PacketInfo info{
        .acl_permission = ACLRole::Permission::NONE,
        .min_args = 1,
        .header = "DE"};
    return info;
}

void PacketDE::handlePacket(AreaData *area, AOClient &client) const
{
    if (!client.checkEvidenceAccess(area))
        return;
    bool is_int = false;
    int l_idx = m_content[0].toInt(&is_int);
    if (is_int && l_idx < area->evidence().size() && l_idx >= 0) {
        area->deleteEvidence(l_idx);
    }
    client.sendEvidenceList(area);
}

bool PacketDE::validatePacket() const
{
    return AkashiUtils::checkArgType<int>(m_content.at(0));
}
