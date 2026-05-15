#include "packet/packet_rd.h"
#include "core/server_settings.h"
#include "server.h"

#include <QDebug>

PacketRD::PacketRD(QStringList &contents) :
    AOPacket(contents)
{
}

PacketInfo PacketRD::packetInfo() const
{
    PacketInfo info{
        .acl_permission = ACLRole::Permission::NONE,
        .min_args = 0,
        .header = "RD"};
    return info;
}

void PacketRD::handlePacket(AreaData *area, AOClient &client) const
{
    if (client.m_hwid == "") {
        // No early connecting!
        client.m_socket->close();
        return;
    }

    if (client.m_joined) {
        return;
    }

    client.m_joined = true;
    client.server()->updateCharsTaken(area);
    client.sendEvidenceList(area);
    client.sendPacket("HP", {"1", QString::number(area->defHP())});
    client.sendPacket("HP", {"2", QString::number(area->proHP())});
    client.sendPacket("FA", client.server()->areaNames());
    // Here lies OPPASS, the genius of FanatSors who send the modpass to everyone in plain text.
    client.sendPacket("DONE");
    client.sendPacket("BN", {area->background(), area->side()});

    client.sendServerMessage("=== MOTD ===\r\n" + client.server()->serverSettings()->motd() + "\r\n=============");

    client.fullArup(); // Give client all the area data
    if (client.server()->timer->isActive()) {
        client.sendPacket("TI", {"0", "2"});
        client.sendPacket("TI", {"0", "0", QString::number(QTime(0, 0).msecsTo(QTime(0, 0).addMSecs(client.server()->timer->remainingTime())))});
    }
    else {
        client.sendPacket("TI", {"0", "3"});
    }
    const QList<QTimer *> l_timers = area->timers();
    for (QTimer *l_timer : l_timers) {
        int l_timer_id = area->timers().indexOf(l_timer) + 1;
        if (l_timer->isActive()) {
            client.sendPacket("TI", {QString::number(l_timer_id), "2"});
            client.sendPacket("TI", {QString::number(l_timer_id), "0", QString::number(QTime(0, 0).msecsTo(QTime(0, 0).addMSecs(l_timer->remainingTime())))});
        }
        else {
            client.sendPacket("TI", {QString::number(l_timer_id), "3"});
        }
    }
    Q_EMIT client.joined();
    area->addClient(-1, client.clientId());
    client.server()->playerStateObserver()->registerClient(&client);
    client.arup(client.ARUPType::PLAYER_COUNT, true); // Tell everyone there is a new player
}
