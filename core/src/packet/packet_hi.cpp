#include "include/packet/packet_hi.h"
#include "include/db_manager.h"
#include "include/server.h"

#include <QDebug>

PacketHI::PacketHI(QStringList &contents) :
    AOPacket(contents)
{
}

PacketInfo PacketHI::getPacketInfo() const
{
    PacketInfo info{
        .acl_permission = ACLRole::Permission::NONE,
        .min_args = 0,
        .header = "HI"};
    return info;
}

void PacketHI::handlePacket(AreaData *area, AOClient &client) const
{
    Q_UNUSED(area)

    QString incoming_hwid = m_content[0];
    if (incoming_hwid.isEmpty() || !client.m_hwid.isEmpty()) {
        // No double sending or empty HWIDs!
        client.sendPacket("BD", {"A protocol error has been encountered. Packet : HI"});
        client.m_socket->close();
        return;
    }

    client.m_hwid = incoming_hwid;
    emit client.getServer()->logConnectionAttempt(client.m_remote_ip.toString(), client.m_ipid, client.m_hwid);
    auto ban = client.getServer()->getDatabaseManager()->isHDIDBanned(client.m_hwid);
    if (ban.first) {
        client.sendPacket("BD", {ban.second + "\nBan ID: " + QString::number(client.getServer()->getDatabaseManager()->getBanID(client.m_hwid))});
        client.m_socket->close();
        return;
    }

    client.sendPacket("ID", {QString::number(client.m_id), "akashi", QCoreApplication::applicationVersion()});
}

bool PacketHI::validatePacket() const
{
    return true;
}
