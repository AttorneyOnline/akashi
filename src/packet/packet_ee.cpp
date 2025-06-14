#include "packet/packet_ee.h"
#include "akashiutils.h"
#include "server.h"

#include <QDebug>
#include <QRegularExpression>

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

    int l_evi_id = m_content[0].toInt();
    if (l_evi_id >= area->evidence().length() || l_evi_id < 0)
        return;

    QString description = m_content[2];

    // Automatically add <owner=all> for evidence in HIDDEN_CM mode areas
    if (area->eviMod() == AreaData::EvidenceMod::HIDDEN_CM) {
        // Check if owner tag already exists in description
        QRegularExpression ownerRegex("<owner=(.*?)>");
        if (!ownerRegex.match(description).hasMatch()) {
            // Add <owner=all> at the beginning if no owner tag exists
            description = "<owner=all>\n" + description;
        }
    }

    AreaData::Evidence evidence;
    evidence.name = m_content[1];
    evidence.description = description;
    evidence.image = m_content[3];

    area->replaceEvidence(l_evi_id, evidence);
    client.sendEvidenceList(area);
}
