#include "packet_ms.h"

ServerPacket::MS_V26::MS_V26(const PacketData &f_data)
{
    const auto &list = std::get<QStringList>(f_data);

    // Core fields (always present)
    setFromList("desk_mod", list, 1, 0);
    setFromList("preanim", list, 2, QString(""));
    setFromList("character", list, 3, QString(""));
    setFromList("emote", list, 4, QString(""));
    setFromList("message", list, 5, QString(""));
    setFromList("side", list, 6, QString("def"));
    setFromList("sfx_name", list, 7, QString(""));
    setFromList("emote_modifier", list, 8, 0);
    setFromList("char_id", list, 9, -1);
    setFromList("sfx_delay", list, 10, 0);
    setFromList("shout_modifier", list, 11, 0);
    setFromList("evidence", list, 12, 0);
    setFromList("flip", list, 13, 0);
    setFromList("realization", list, 14, 0);
    setFromList("text_color", list, 15, 0);

    setFromList("showname", list, 16, QString(""));
    setFromList("other_charid", list, 17, -1);
    setFromList("other_name", list, 18, QString(""));
    setFromList("other_emote", list, 19, QString(""));
    setFromList("self_offset", list, 20, QString("0"));
    setFromList("noninterrupting_preanim", list, 21, 0);
}

ClientPacket::MS_V26::MS_V26(const PacketData &f_data)
{
    const auto &list = std::get<QStringList>(f_data);

    // Core fields
    setFromList("desk_mod", list, 1, 0);
    setFromList("preanim", list, 2, QString(""));
    setFromList("character", list, 3, QString(""));
    setFromList("emote", list, 4, QString(""));
    setFromList("message", list, 5, QString(""));
    setFromList("side", list, 6, QString("def"));
    setFromList("sfx_name", list, 7, QString(""));
    setFromList("emote_modifier", list, 8, 0);
    setFromList("char_id", list, 9, -1);
    setFromList("sfx_delay", list, 10, 0);
    setFromList("shout_modifier", list, 11, 0);
    setFromList("evidence", list, 12, 0);
    setFromList("flip", list, 13, 0);
    setFromList("realization", list, 14, 0);
    setFromList("text_color", list, 15, 0);

    // 2.6 fields (no other_name, other_emote)
    setFromList("showname", list, 16, QString(""));
    setFromList("other_charid", list, 17, -1);
    setFromList("self_offset", list, 18, QString("0"));
    setFromList("noninterrupting_preanim", list, 19, 0);
}
