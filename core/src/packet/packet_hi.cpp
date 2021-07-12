#include "include/packet/packet_hi.h"

PacketHDID::PacketHDID(QStringList p_contents)
    : AOPacket(p_contents)
{
    header = "HI";
    acl_mask = ACLFlags.value("NONE");
    min_args = 1;
}

void PacketHDID::handlePacket(AreaData* area, AOClient& client)
{
    client.hwid = contents[0];
    auto ban = client.server->db_manager->isHDIDBanned(client.hwid);
    if (ban.first) {
        client.sendPacket("BD", {ban.second + "\nBan ID: " + QString::number(client.server->db_manager->getBanID(client.hwid))});
        client.socket->close();
        return;
    }
    client.sendPacket("ID", {QString::number(client.id), "akashi", QCoreApplication::applicationVersion()});
}