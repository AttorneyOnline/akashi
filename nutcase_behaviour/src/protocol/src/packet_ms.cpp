#include "packet_ms.h"
#include "packet_decoder.h"

ServerPacket::MS_V26::MS_V26(const PacketData &f_data)
{
    setHeader("MS");

    if (std::holds_alternative<QStringList>(f_data))
    {
        const auto &list = std::get<QStringList>(f_data);

        // Use PacketDecoder for cleaner code
        PacketDecoder(*this)
            .fromList(list)
            .field<int>("desk_mod", 1, 0)
            .field<QString>("preanim", 2, QString(""))
            .field<QString>("character", 3, QString(""))
            .field<QString>("emote", 4, QString(""))
            .field<QString>("message", 5, QString(""))
            .field<QString>("side", 6, QString("def"))
            .field<QString>("sfx_name", 7, QString(""))
            .field<int>("emote_modifier", 8, 0)
            .field<int>("char_id", 9, -1)
            .field<int>("sfx_delay", 10, 0)
            .field<int>("shout_modifier", 11, 0)
            .field<int>("evidence", 12, 0)
            .field<int>("flip", 13, 0)
            .field<int>("realization", 14, 0)
            .field<int>("text_color", 15, 0)
            .field<QString>("showname", 16, QString(""))
            .field<int>("other_charid", 17, -1)
            .field<QString>("other_name", 18, QString(""))
            .field<QString>("other_emote", 19, QString(""))
            .field<QString>("self_offset", 20, QString("0"))
            .field<int>("noninterrupting_preanim", 21, 0);
    }
    else
    {
        const auto &json = std::get<QJsonObject>(f_data);

        PacketDecoder(*this)
            .fromJson(json)
            .field<int>("desk_mod", 0)
            .field<QString>("preanim", QString(""))
            .field<QString>("character", QString(""))
            .field<QString>("emote", QString(""))
            .field<QString>("message", QString(""))
            .field<QString>("side", QString("def"))
            .field<QString>("sfx_name", QString(""))
            .field<int>("emote_modifier", 0)
            .field<int>("char_id", -1)
            .field<int>("sfx_delay", 0)
            .field<int>("shout_modifier", 0)
            .field<int>("evidence", 0)
            .field<int>("flip", 0)
            .field<int>("realization", 0)
            .field<int>("text_color", 0)
            .field<QString>("showname", QString(""))
            .field<int>("other_charid", -1)
            .field<QString>("other_name", QString(""))
            .field<QString>("other_emote", QString(""))
            .field<QString>("self_offset", QString("0"))
            .field<int>("noninterrupting_preanim", 0);
    }
}

ClientPacket::MS_V26::MS_V26(const PacketData &f_data)
{
    setHeader("MS");

    if (std::holds_alternative<QStringList>(f_data))
    {
        const auto &list = std::get<QStringList>(f_data);

        PacketDecoder(*this)
            .fromList(list)
            .field<int>("desk_mod", 1, 0)
            .field<QString>("preanim", 2, QString(""))
            .field<QString>("character", 3, QString(""))
            .field<QString>("emote", 4, QString(""))
            .field<QString>("message", 5, QString(""))
            .field<QString>("side", 6, QString("def"))
            .field<QString>("sfx_name", 7, QString(""))
            .field<int>("emote_modifier", 8, 0)
            .field<int>("char_id", 9, -1)
            .field<int>("sfx_delay", 10, 0)
            .field<int>("shout_modifier", 11, 0)
            .field<int>("evidence", 12, 0)
            .field<int>("flip", 13, 0)
            .field<int>("realization", 14, 0)
            .field<int>("text_color", 15, 0)
            .field<QString>("showname", 16, QString(""))
            .field<int>("other_charid", 17, -1)
            .field<QString>("self_offset", 18, QString("0"))
            .field<int>("noninterrupting_preanim", 19, 0);
    }
    else
    {
        const auto &json = std::get<QJsonObject>(f_data);

        PacketDecoder(*this)
            .fromJson(json)
            .field<int>("desk_mod", 0)
            .field<QString>("preanim", QString(""))
            .field<QString>("character", QString(""))
            .field<QString>("emote", QString(""))
            .field<QString>("message", QString(""))
            .field<QString>("side", QString("def"))
            .field<QString>("sfx_name", QString(""))
            .field<int>("emote_modifier", 0)
            .field<int>("char_id", -1)
            .field<int>("sfx_delay", 0)
            .field<int>("shout_modifier", 0)
            .field<int>("evidence", 0)
            .field<int>("flip", 0)
            .field<int>("realization", 0)
            .field<int>("text_color", 0)
            .field<QString>("showname", QString(""))
            .field<int>("other_charid", -1)
            .field<QString>("self_offset", QString("0"))
            .field<int>("noninterrupting_preanim", 0);
    }
}
