#include "packet/packet_pe.h"
#include "server.h"

#include <QDebug>
#include <QRegularExpression>

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

    QString description = m_content[1];

    // Automatically add <owner=all> for evidence in HIDDEN_CM mode areas
    if (area->eviMod() == AreaData::EvidenceMod::HIDDEN_CM) {
        // Check if owner tag already exists in description
        static const QRegularExpression ownerRegex("<owner=(.*?)>");
        if (!ownerRegex.match(description).hasMatch()) {
            // Add <owner=all> at the beginning if no owner tag exists
            description = "<owner=all>\n" + description;
        }
    }

    AreaData::Evidence evidence;
    evidence.name = m_content[0];
    evidence.description = description;
    evidence.image = m_content[2];

    area->appendEvidence(evidence);
    client.sendEvidenceList(area);
}
