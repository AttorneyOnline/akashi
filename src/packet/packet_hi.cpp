#include "packet/packet_hi.h"
#include "akashiutils.h"
#include "db_manager.h"
#include "server.h"

#include <QDebug>

PacketHI::PacketHI(QStringList &contents) :
    AOPacket(contents)
{
}

PacketInfo PacketHI::getPacketInfo() const
{
    PacketInfo info{
        .acl_permission = ACLRole::Permission::NONE,
        .min_args = 1,
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
        QString ban_duration;
        if (!(ban.second.duration == -2)) {
            ban_duration = QDateTime::fromSecsSinceEpoch(ban.second.time).addSecs(ban.second.duration).toString("MM/dd/yyyy, hh:mm");
        }
        else {
            ban_duration = "Permanently.";
        }
        client.sendPacket("BD", {"Reason: " + ban.second.reason + "\nBan ID: " + QString::number(ban.second.id) + "\nUntil: " + ban_duration});
        client.m_socket->close();
        return;
    }

    client.sendPacket("ID", {QString::number(client.clientId()), "akashi", QCoreApplication::applicationVersion()});
}
