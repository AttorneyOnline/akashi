#include "packet_pl.h"

#include <QJsonDocument>

PacketPL::PacketPL(QStringList &contents) :
    AOPacket(contents)
{}

PacketPL::PacketPL(const QList<PlayerData> &f_player_list) :
    AOPacket(QStringList{""})
{

    QJsonArray player_list_json;
    for (const PlayerData &player : f_player_list) {
        QJsonObject player_json;

        player_json["id"] = player.id;
        player_json["name"] = player.name;
        player_json["character"] = player.character;
        player_json["character_name"] = player.character_name;
        player_json["area_id"] = player.area_id;

        player_list_json.append(player_json);
    }

    setContentField(0, QJsonDocument(player_list_json).toJson(QJsonDocument::Compact));
}

PacketInfo PacketPL::getPacketInfo() const { return PacketInfo{.acl_permission = ACLRole::NONE, .min_args = 1, .header = "PL"}; }

void PacketPL::handlePacket(AreaData *area, AOClient &client) const
{
    Q_UNUSED(area);
    Q_UNUSED(client);
}

PacketPLU::PacketPLU(QStringList &contents) :
    AOPacket(contents)
{}

PacketPLU::PacketPLU(int f_id, UpdateType f_type) :
    AOPacket(QStringList{""})
{
    QJsonObject data_json;
    data_json["id"] = f_id;
    data_json["type"] = f_type;

    setContentField(0, QJsonDocument(data_json).toJson(QJsonDocument::Compact));
}

PacketInfo PacketPLU::getPacketInfo() const { return PacketInfo{.acl_permission = ACLRole::NONE, .min_args = 1, .header = "PLU"}; }

void PacketPLU::handlePacket(AreaData *area, AOClient &client) const
{
    Q_UNUSED(area);
    Q_UNUSED(client);
}

PacketPU::PacketPU(QStringList &contents) :
    AOPacket(contents)
{}

PacketPU::PacketPU(int f_id, DataType f_type, const QString &f_data) :
    AOPacket(QStringList{""})
{
    QJsonObject data_json;
    data_json["id"] = f_id;
    data_json["type"] = f_type;
    data_json["data"] = f_data;

    setContentField(0, QJsonDocument(data_json).toJson(QJsonDocument::Compact));
}

PacketPU::PacketPU(int f_id, DataType f_type, int f_data) :
    PacketPU(f_id, f_type, QString::number(f_data))
{
}

PacketInfo PacketPU::getPacketInfo() const
{
    return PacketInfo{.acl_permission = ACLRole::NONE, .min_args = 1, .header = "PU"};
}

void PacketPU::handlePacket(AreaData *area, AOClient &client) const
{
    Q_UNUSED(area);
    Q_UNUSED(client);
}
