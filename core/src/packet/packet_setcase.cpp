#include "include/packet/packet_setcase.h"
#include "include/server.h"

#include <QDebug>

PacketSetcase::PacketSetcase(QStringList &contents) :
    AOPacket(contents)
{
}

PacketInfo PacketSetcase::getPacketInfo() const
{
    PacketInfo info{
        .acl_permission = ACLRole::Permission::NONE,
        .min_args = 7,
        .header = "Setcase"};
    return info;
}

void PacketSetcase::handlePacket(AreaData *area, AOClient &client) const
{
    Q_UNUSED(area)

    QList<bool> l_prefs_list;
    for (int i = 2; i <= 6; i++) {
        bool is_int = false;
        bool pref = m_content[i].toInt(&is_int);
        if (!is_int)
            return;
        l_prefs_list.append(pref);
    }
    client.m_casing_preferences = l_prefs_list;
}

bool PacketSetcase::validatePacket() const
{
    return true;
}
