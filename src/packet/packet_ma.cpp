#include "packet_ma.h"

#include "config_manager.h"
#include "db_manager.h"
#include "server.h"

PacketMA::PacketMA(QStringList &contents) :
    AOPacket(contents)
{
}

PacketInfo PacketMA::getPacketInfo() const
{
    PacketInfo info{
        .acl_permission = ACLRole::Permission::NONE,
        .min_args = 3,
        .header = "MA"};
    return info;
}

void PacketMA::handlePacket(AreaData *area, AOClient &client) const
{
    if (!client.m_authenticated) {
        client.sendServerMessage("You are not logged in!");
        return;
    }

    int client_id = m_content.at(0).toInt();
    int duration = qMax(m_content.at(1).toInt(), -1);
    QString reason = m_content.at(2);

    bool is_kick = duration == 0;
    if (is_kick) {
        if (!client.checkPermission(ACLRole::KICK)) {
            client.sendServerMessage("You do not have permission to kick users.");
            return;
        }
    }
    else {
        if (!client.checkPermission(ACLRole::BAN)) {
            client.sendServerMessage("You do not have permission to ban users.");
            return;
        }
    }

    AOClient *target = client.getServer()->getClientByID(client_id);
    if (target == nullptr) {
        client.sendServerMessage("User not found.");
        return;
    }

    QString moderator_name;
    if (ConfigManager::authType() == DataTypes::AuthType::ADVANCED) {
        moderator_name = client.m_moderator_name;
    }
    else {
        moderator_name = "Moderator";
    }

    QList<AOClient *> clients = client.getServer()->getClientsByIpid(target->m_ipid);
    if (is_kick) {
        for (AOClient *subclient : clients) {
            subclient->sendPacket("KK", {reason});
            subclient->m_socket->close();
        }

        Q_EMIT client.logKick(moderator_name, target->m_ipid, reason);

        client.sendServerMessage("Kicked " + QString::number(clients.size()) + " client(s) with ipid " + target->m_ipid + " for reason: " + reason);
    }
    else {
        DBManager::BanInfo ban;

        ban.ip = target->m_remote_ip;
        ban.ipid = target->m_ipid;
        ban.moderator = moderator_name;
        ban.reason = reason;
        ban.time = QDateTime::currentDateTime().toSecsSinceEpoch();

        QString timestamp;
        if (duration == -1) {
            ban.duration = -2;
            timestamp = "permanently";
        }
        else {
            ban.duration = duration * 60;
            timestamp = QDateTime::fromSecsSinceEpoch(ban.time).addSecs(ban.duration).toString("MM/dd/yyyy, hh:mm");
        }

        for (AOClient *subclient : clients) {
            ban.hdid = subclient->m_hwid;

            client.getServer()->getDatabaseManager()->addBan(ban);

            subclient->sendPacket("KB", {reason});
            subclient->m_socket->close();
        }

        if (ban.duration == -2) {
            timestamp = "permanently";
        }
        else {
            timestamp = QString::number(ban.time + ban.duration);
        }

        Q_EMIT client.logBan(moderator_name, target->m_ipid, timestamp, reason);

        client.sendServerMessage("Banned " + QString::number(clients.size()) + " client(s) with ipid " + target->m_ipid + " for reason: " + reason);

        int ban_id = client.getServer()->getDatabaseManager()->getBanID(ban.ip);
        if (ConfigManager::discordBanWebhookEnabled()) {
            Q_EMIT client.getServer()->banWebhookRequest(ban.ipid, ban.moderator, timestamp, ban.reason, ban_id);
        }
    }
}
