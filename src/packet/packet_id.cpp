#include "packet/packet_id.h"

#include "akashidefs.h"
#include "config_manager.h"
#include "server.h"

#include <QDebug>

PacketID::PacketID(QStringList &contents) :
    AOPacket(contents)
{
}

PacketInfo PacketID::getPacketInfo() const
{
    PacketInfo info{
        .acl_permission = ACLRole::Permission::NONE,
        .min_args = 3,
        .header = "ID"};
    return info;
}

void PacketID::handlePacket(AreaData *area, AOClient &client) const
{
    Q_UNUSED(area)

    if (client.m_version.major == akashi::PROTOCOL_MAJOR_VERSION) {
        // No double sending of the ID packet!
        client.sendPacket("BD", {"A protocol error has been encountered. Packet : ID"});
        client.m_socket->close();
        return;
    }

    if (!ConfigManager::webaoEnabled() && m_content[0] == "webAO") {
        client.sendPacket("BD", {"WebAO is disabled on this server."});
        client.m_socket->close();
        return;
    }

    AOClient::ClientVersion version;
    if (m_content[2] == akashi::get_protocol_version_string()) {
        version.major = akashi::PROTOCOL_MAJOR_VERSION;
        version.minor = akashi::PROTOCOL_MINOR_VERSION;
        version.patch = akashi::PROTOCOL_PATCH_VERSION;
    }
    else {
        client.sendPacket("BD", {"A protocol error has been encountered. Packet : ID\nProtocol version not supported."});
        client.m_socket->close();
        return;
    }

    client.sendPacket("ID", {QString::number(client.clientId()), "akashi", QCoreApplication::applicationVersion()});
}
