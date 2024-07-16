#include "packet/packet_id.h"

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
        .min_args = 2,
        .header = "ID"};
    return info;
}

void PacketID::handlePacket(AreaData *area, AOClient &client) const
{
    Q_UNUSED(area)

    if (client.m_version.release == 2) {
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

    QRegularExpression rx("\\b(\\d+)\\.(\\d+)\\.(\\d+)\\b"); // matches X.X.X (e.g. 2.9.0, 2.4.10, etc.)
    QRegularExpressionMatch l_match = rx.match(m_content[1]);
    if (l_match.hasMatch()) {
        client.m_version.release = l_match.captured(1).toInt();
        client.m_version.major = l_match.captured(2).toInt();
        client.m_version.minor = l_match.captured(3).toInt();
    }

    if (client.m_version.release != 2) {
        client.sendPacket("BD", {"A protocol error has been encountered. Packet : ID\nRelease version not recognised."});
        client.m_socket->close();
        return;
    }

    client.sendPacket("PN", {QString::number(client.getServer()->getPlayerCount()), QString::number(ConfigManager::maxPlayers()), ConfigManager::serverDescription()});

    QStringList l_feature_list = {
        "noencryption", "yellowtext", "prezoom",
        "flipping", "customobjections", "fastloading",
        "deskmod", "evidence", "cccc_ic_support",
        "arup", "casing_alerts", "modcall_reason",
        "looping_sfx", "additive", "effects",
        "y_offset", "expanded_desk_mods", "auth_packet", "custom_blips"};
    client.sendPacket("FL", l_feature_list);

    if (ConfigManager::assetUrl().isValid()) {
        QByteArray l_asset_url = ConfigManager::assetUrl().toEncoded(QUrl::EncodeSpaces);
        client.sendPacket("ASS", {l_asset_url});
    }
}
