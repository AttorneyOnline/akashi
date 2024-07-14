#include "packet_pr.h"

PacketPR::PacketPR(QStringList &contents) :
    AOPacket(contents)
{}

PacketPR::PacketPR(int f_id, UPDATE_TYPE f_update) :
    AOPacket(QStringList{QString::number(f_id), QString::number(f_update)})
{}

PacketInfo PacketPR::getPacketInfo() const { return PacketInfo{.acl_permission = ACLRole::NONE, .min_args = 2, .header = "PR"}; }

void PacketPR::handlePacket(AreaData *area, AOClient &client) const
{
    Q_UNUSED(area);
    Q_UNUSED(client);
}

PacketPU::PacketPU(QStringList &contents) :
    AOPacket(contents)
{}

PacketPU::PacketPU(int f_id, DATA_TYPE f_type, const QString &f_data) :
    AOPacket(QStringList{QString::number(f_id), QString::number(f_type), f_data})
{}

PacketPU::PacketPU(int f_id, DATA_TYPE f_type, int f_data) :
    PacketPU(f_id, f_type, QString::number(f_data))
{
}

PacketInfo PacketPU::getPacketInfo() const
{
    return PacketInfo{.acl_permission = ACLRole::NONE, .min_args = 3, .header = "PU"};
}

void PacketPU::handlePacket(AreaData *area, AOClient &client) const
{
    Q_UNUSED(area);
    Q_UNUSED(client);
}
