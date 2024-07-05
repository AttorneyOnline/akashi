#include "packet/packet_casea.h"
#include "akashiutils.h"
#include "packet/packet_factory.h"
#include "server.h"

#include <QDebug>

PacketCasea::PacketCasea(QStringList &contents) :
    AOPacket(contents)
{
}

PacketInfo PacketCasea::getPacketInfo() const
{
    PacketInfo info{
        .acl_permission = ACLRole::Permission::NONE,
        .min_args = 6,
        .header = "CASEA"};
    return info;
}

void PacketCasea::handlePacket(AreaData *area, AOClient &client) const
{
    Q_UNUSED(area)

    QString l_case_title = m_content[0];
    QStringList l_needed_roles;
    QList<bool> l_needs_list;
    for (int i = 1; i <= 5; i++) {
        bool is_int = false;
        bool need = m_content[i].toInt(&is_int);
        if (!is_int)
            return;
        l_needs_list.append(need);
    }
    QStringList l_roles = {"defense attorney", "prosecutor", "judge", "jurors", "stenographer"};
    for (int i = 0; i < 5; i++) {
        if (l_needs_list[i])
            l_needed_roles.append(l_roles[i]);
    }
    if (l_needed_roles.isEmpty())
        return;

    QString l_message = "=== Case Announcement ===\r\n" + (client.name() == "" ? client.character() : client.name()) + " needs " + l_needed_roles.join(", ") + " for " + (l_case_title == "" ? "a case" : l_case_title) + "!";

    QList<AOClient *> l_clients_to_alert;
    QSet<bool> l_needs_set(l_needs_list.begin(), l_needs_list.end());

    const QVector<AOClient *> l_clients = client.getServer()->getClients();
    for (AOClient *l_client : l_clients) {
        QSet<bool> l_matches(l_client->m_casing_preferences.begin(), l_client->m_casing_preferences.end());
        l_matches.intersect(l_needs_set);

        if (!l_matches.isEmpty() && !l_clients_to_alert.contains(l_client))
            l_clients_to_alert.append(l_client);
    }

    for (AOClient *l_client : l_clients_to_alert) {
        l_client->sendPacket(PacketFactory::createPacket("CASEA", {l_message, m_content[1], m_content[2], m_content[3], m_content[4], m_content[5], "1"}));
        // you may be thinking, "hey wait a minute the network protocol documentation doesn't mention that last argument!"
        // if you are in fact thinking that, you are correct! it is not in the documentation!
        // however for some inscrutable reason Attorney Online 2 will outright reject a CASEA packet that does not have
        // at least 7 arguments despite only using the first 6. Cera, i kneel. you have truly broken me.
    }
}
