#include "include/packet/packet_ee.h"
#include "include/akashiutils.h"
#include "include/server.h"

#include <QDebug>

PacketEE::PacketEE(QStringList &contents) :
    AOPacket(contents)
{
}

PacketInfo PacketEE::getPacketInfo() const
{
    PacketInfo info{
        .acl_permission = ACLRole::Permission::NONE,
        .min_args = 4,
        .header = "EE"};
    return info;
}

void PacketEE::handlePacket(AreaData *area, AOClient &client) const
{
    if (!client.checkEvidenceAccess(area))
        return;
    bool is_int = false;
    int l_idx = m_content[0].toInt(&is_int);
    AreaData::Evidence l_evi = {m_content[1], m_content[2], m_content[3]};
    if (is_int && l_idx < area->evidence().size() && l_idx >= 0) {
        area->replaceEvidence(l_idx, l_evi);
    }
    client.sendEvidenceList(area);
}

bool PacketEE::validatePacket() const
{
    return AkashiUtils::checkArgType<int>(m_content.at(0));
}
