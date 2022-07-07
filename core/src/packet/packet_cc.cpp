#include "include/packet/packet_cc.h"
#include "include/akashiutils.h"
#include "include/config_manager.h"
#include "include/server.h"

#include <QDebug>

PacketCC::PacketCC(QStringList &contents) :
    AOPacket(contents)
{
}

PacketInfo PacketCC::getPacketInfo() const
{
    PacketInfo info{
        .acl_permission = ACLRole::Permission::NONE,
        .min_args = 3,
        .header = "CC"};
    return info;
}

void PacketCC::handlePacket(AreaData *area, AOClient &client) const
{
    Q_UNUSED(area)

    bool argument_ok;
    int l_selected_char_id = m_content[1].toInt(&argument_ok);
    if (!argument_ok) {
        l_selected_char_id = client.SPECTATOR_ID;
    }

    if (l_selected_char_id < -1 || l_selected_char_id > client.getServer()->getCharacters().size() - 1) {
        client.sendPacket("KK", {"A protocol error has been encountered.Packet : CC\nCharacter ID out of range."});
        client.m_socket->close();
    }

    if (client.changeCharacter(l_selected_char_id))
        client.m_char_id = l_selected_char_id;

    if (client.m_char_id > client.SPECTATOR_ID) {
        client.setSpectator(false);
    }
}

bool PacketCC::validatePacket() const
{
    return AkashiUtils::checkArgType<int>(m_content.at(1));
}
