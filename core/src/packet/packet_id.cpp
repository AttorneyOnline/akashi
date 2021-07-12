#include "include/packet/packet_id.h"

PacketID::PacketID(QStringList p_contents)
    : AOPacket(p_contents)
{
    header = "ID";
    acl_mask = ACLFlags.value("NONE");
    min_args = 0;
}

void PacketID::handlePacket(AreaData* area, AOClient& client) {


    // Full feature list as of AO 2.8.5
    // The only ones that are critical to ensuring the server works are
    // "noencryption" and "fastloading"
    QStringList feature_list = {
        "noencryption", "yellowtext",         "prezoom",
        "flipping",     "customobjections",   "fastloading",
        "deskmod",      "evidence",           "cccc_ic_support",
        "arup",         "casing_alerts",      "modcall_reason",
        "looping_sfx",  "additive",           "effects",
        "y_offset",     "expanded_desk_mods", "auth_packet"
    };


    client.version.string = contents[1];
    QRegularExpression rx("\\b(\\d+)\\.(\\d+)\\.(\\d+)\\b"); // matches X.X.X (e.g. 2.9.0, 2.4.10, etc.)
    QRegularExpressionMatch match = rx.match(client.version.string);
    if (match.hasMatch()) {
        client.version.release = match.captured(1).toInt();
        client.version.major = match.captured(2).toInt();
        client.version.minor = match.captured(3).toInt();
    }

    client.sendPacket("PN", {QString::number(client.server->player_count), QString::number(ConfigManager::maxPlayers())});
    client.sendPacket("FL", feature_list);

    if (ConfigManager::assetUrl().isValid()) {
        QByteArray asset_url = ConfigManager::assetUrl().toEncoded(QUrl::EncodeSpaces);
        client.sendPacket("ASS", {asset_url});
    }
}