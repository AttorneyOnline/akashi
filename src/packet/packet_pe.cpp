#include "packet/packet_pe.h"
#include "server.h"

#include <QDebug>

PacketPE::PacketPE(QStringList &contents) :
    AOPacket(contents)
{
}

PacketInfo PacketPE::getPacketInfo() const
{
    PacketInfo info{
        .acl_permission = ACLRole::Permission::NONE,
        .min_args = 3,
        .header = "PE"};
    return info;
}

void PacketPE::handlePacket(AreaData *area, AOClient &client) const
{
    if (!client.checkEvidenceAccess(area))
        return;
    AreaData::Evidence l_evi = {m_content[0], m_content[1], m_content[2]};
    area->appendEvidence(l_evi);
    client.sendEvidenceList(area);
}

bool PacketPE::validatePacket() const
{
    return true;
}
