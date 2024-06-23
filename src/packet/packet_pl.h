#pragma once

#include "network/aopacket.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>

class PacketPL : public AOPacket
{
  public:
    struct PlayerData
    {
        int id;
        QString name;
        QString character;
        QString character_name;
        int area_id = -1;
    };

    PacketPL(QStringList &contents);
    PacketPL(const QList<PlayerData> &f_player_list);
    PacketInfo getPacketInfo() const override;
    void handlePacket(AreaData *area, AOClient &client) const override;
};

class PacketPLU : public AOPacket
{
  public:
    enum UpdateType
    {
        AddPlayerUpdate,
        RemovePlayerUpdate,
    };

    PacketPLU(QStringList &contents);
    PacketPLU(int f_id, UpdateType f_type);
    PacketInfo getPacketInfo() const override;
    void handlePacket(AreaData *area, AOClient &client) const override;
};

class PacketPU : public AOPacket
{
  public:
    enum DataType
    {
        NameData,
        CharacterData,
        CharacterNameData,
        AreaIdData,
    };

    PacketPU(QStringList &contents);
    PacketPU(int f_id, DataType f_type, const QString &f_data);
    PacketPU(int f_id, DataType f_type, int f_data);
    PacketInfo getPacketInfo() const override;
    void handlePacket(AreaData *area, AOClient &client) const override;
};
