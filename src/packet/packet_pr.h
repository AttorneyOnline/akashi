#pragma once

#include "network/aopacket.h"

#include <QString>

class PacketPR : public AOPacket
{
  public:
    enum UPDATE_TYPE
    {
        ADD,
        REMOVE,
    };

    PacketPR(QStringList &contents);
    PacketPR(int f_id, UPDATE_TYPE f_update);
    PacketInfo getPacketInfo() const override;
    void handlePacket(AreaData *area, AOClient &client) const override;
};

class PacketPU : public AOPacket
{
  public:
    enum DATA_TYPE
    {
        NAME,
        CHARACTER,
        CHARACTER_NAME,
        AREA_ID,
    };

    PacketPU(QStringList &contents);
    PacketPU(int f_id, DATA_TYPE f_type, const QString &f_data);
    PacketPU(int f_id, DATA_TYPE f_type, int f_data);
    PacketInfo getPacketInfo() const override;
    void handlePacket(AreaData *area, AOClient &client) const override;
};
