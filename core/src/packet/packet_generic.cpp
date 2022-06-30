#include "include/packet/packet_generic.h"

#include <QDebug>

PacketGeneric::PacketGeneric(QString header, QStringList contents) :
    AOPacket(contents),
    header(header)
{
}

PacketInfo PacketGeneric::getPacketInfo() const
{
    PacketInfo info{
        .acl_permission = ACLRole::Permission::NONE,
        .min_args = 0,
        .header = header};
    return info;
}

void PacketGeneric::handlePacket(AreaData *area, AOClient &client) const
{
    Q_UNUSED(area)
    Q_UNUSED(client)
    qDebug() << "ERROR: Cannot handle generic packet: " << header;
    qDebug() << "Packet is either unimplemented, or is meant to be sent to client";
}

bool PacketGeneric::validatePacket() const
{
    return true;
}
