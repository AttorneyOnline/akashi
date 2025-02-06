#include "packet/data/msdata.h"

#include <QJsonArray>

bool ms2::OldMSFlatData::fromJson(const QJsonObject &f_json, OldMSFlatData &f_data)
{
    if (const QJsonValue v = f_json["desk_mod"]; v.isDouble())
        f_data.m_desk_mod = DeskMod(v.toInt());
    else
        return false;

    if (const QJsonValue v = f_json["preanim"]; v.isString())
        f_data.m_preanim = v.toString();
    else
        return false;

    if (const QJsonValue v = f_json["char_name"]; v.isString())
        f_data.m_char_name = v.toString();
    else
        return false;

    if (const QJsonValue v = f_json["emote"]; v.isString())
        f_data.m_emote = v.toString();
    else
        return false;

    if (const QJsonValue v = f_json["message_text"]; v.isString())
        f_data.m_message_text = v.toString();
    else
        return false;

    if (const QJsonValue v = f_json["side"]; v.isString()) {
        f_data.m_side = v.toString();
        f_data.m_side.replace("../", "").replace("..\\", "");
    }
    else
        return false;

    if (const QJsonValue v = f_json["sfx_name"]; v.isString())
        f_data.m_sfx_name = v.toString();
    else
        return false;

    if (const QJsonValue v = f_json["emote_mod"]; v.isDouble())
        f_data.m_emote_mod = EmoteMod(v.toInt());
    else
        return false;

    if (const QJsonValue v = f_json["char_id"]; v.isDouble())
        f_data.m_char_id = v.toInt();
    else
        return false;

    if (const QJsonValue v = f_json["sfx_delay"]; v.isDouble())
        f_data.m_sfx_delay = v.toInt();
    else
        return false;

    if (const QJsonValue v = f_json["objection_mod"]; v.isDouble()) {
        f_data.m_objection_mod = ObjectionMod(v.toInt());
        if (f_data.m_objection_mod == ObjectionMod::Custom) {
            if (const QJsonValue v = f_json["objection_custom"]; v.isString())
                f_data.m_objection_custom = v.toString();
        }
    }
    else
        return false;

    if (const QJsonValue v = f_json["evidence"]; v.isDouble())
        f_data.m_evidence = v.toInt();
    else
        return false;

    if (const QJsonValue v = f_json["flip"]; v.isBool())
        f_data.m_flip = v.toBool();
    else
        return false;

    if (const QJsonValue v = f_json["realisation"]; v.isBool())
        f_data.m_realisation = v.toBool();
    else
        return false;

    if (const QJsonValue v = f_json["text_colour"]; v.isDouble())
        f_data.m_text_colour = v.toInt();
    else
        return false;

    if (const QJsonValue v = f_json["showname"]; v.isString())
        f_data.m_showname = v.toString();

    if (const QJsonValue v = f_json["other_charid"]; v.isDouble())
        f_data.m_other_charid = v.toInt();

    if (const QJsonValue v = f_json["other_name"]; v.isString())
        f_data.m_other_name = v.toString();

    if (const QJsonValue v = f_json["self_x_offset"]; v.isDouble())
        f_data.m_self_x_offset = v.toInt();

    if (const QJsonValue v = f_json["self_y_offset"]; v.isDouble())
        f_data.m_self_y_offset = v.toInt();

    if (const QJsonValue v = f_json["other_x_offset"]; v.isDouble())
        f_data.m_other_x_offset = v.toInt();

    if (const QJsonValue v = f_json["other_y_offset"]; v.isDouble())
        f_data.m_other_y_offset = v.toInt();

    if (const QJsonValue v = f_json["other_flip"]; v.isBool())
        f_data.m_other_flip = v.toBool();

    if (const QJsonValue v = f_json["immediate"]; v.isBool())
        f_data.m_immediate = v.toBool();

    if (const QJsonValue v = f_json["sfx_looping"]; v.isBool())
        f_data.m_sfx_looping = v.toBool();

    if (const QJsonValue v = f_json["screenshake"]; v.isBool())
        f_data.m_screenshake = v.toBool();

    if (const QJsonValue v = f_json["frames_shake"]; v.isArray()) {
        const QJsonArray frames = v.toArray();
        f_data.m_frames_shake.reserve(frames.size());
        for (const QJsonValue &frame : frames) {
            FrameData l_data{};
            if (frame.isObject() && FrameData::fromJson(frame.toObject(), l_data))
                f_data.m_frames_shake.append(l_data);
        }
    }

    if (const QJsonValue v = f_json["frames_realisation"]; v.isArray()) {
        const QJsonArray frames = v.toArray();
        f_data.m_frames_realisation.reserve(frames.size());
        for (const QJsonValue &frame : frames) {
            FrameData l_data{};
            if (frame.isObject() && FrameData::fromJson(frame.toObject(), l_data))
                f_data.m_frames_realisation.append(l_data);
        }
    }

    if (const QJsonValue v = f_json["frames_sfx"]; v.isArray()) {
        const QJsonArray frames = v.toArray();
        f_data.m_frames_sfx.reserve(frames.size());
        for (const QJsonValue &frame : frames) {
            FrameData l_data{};
            if (frame.isObject() &&
                FrameData::fromJson(frame.toObject(), l_data) &&
                !l_data.m_value.isEmpty())
                f_data.m_frames_sfx.append(l_data);
        }
    }

    if (const QJsonValue v = f_json["additive"]; v.isBool())
        f_data.m_additive = v.toBool();

    if (const QJsonValue v = f_json["effect"]; v.isString())
        f_data.m_effect = v.toString();

    if (const QJsonValue v = f_json["blips"]; v.isString())
        f_data.m_blips = v.toString();

    return true;
}

QJsonObject ms2::OldMSFlatData::toJson() const
{
    QJsonObject json;

    json["desk_mod"] = static_cast<qint32>(m_desk_mod);
    json["preanim"] = m_preanim;
    json["char_name"] = m_char_name;
    json["emote"] = m_emote;
    json["message_text"] = m_message_text;
    json["side"] = m_side;
    json["sfx_name"] = m_sfx_name;
    json["emote_mod"] = static_cast<qint32>(m_emote_mod);
    json["char_id"] = m_char_id;
    json["sfx_delay"] = m_sfx_delay;
    json["objection_mod"] = static_cast<qint32>(m_objection_mod);
    json["objection_custom"] = m_objection_custom;
    json["evidence"] = m_evidence;
    json["flip"] = m_flip;
    json["realisation"] = m_realisation;
    json["text_color"] = m_text_colour;
    json["showname"] = m_showname;
    json["other_charid"] = m_other_charid;
    json["other_name"] = m_other_name;
    json["other_emote"] = m_other_emote;
    json["self_x_offset"] = m_self_x_offset;
    json["self_y_offset"] = m_self_y_offset;
    json["other_x_offset"] = m_other_x_offset;
    json["other_y_offset"] = m_other_y_offset;
    json["other_flip"] = m_other_flip;
    json["immediate"] = m_immediate;
    json["sfx_looping"] = m_sfx_looping;
    json["screenshake"] = m_screenshake;

    {
        QJsonArray frameShakeArray;
        for (const auto &frame : m_frames_shake)
            frameShakeArray.append(frame.toJson());
        json["frames_shake"] = frameShakeArray;
    }
    {
        QJsonArray frameRealisationArray;
        for (const auto &frame : m_frames_realisation)
            frameRealisationArray.append(frame.toJson());
        json["frames_realisation"] = frameRealisationArray;
    }
    {
        QJsonArray frameSfxArray;
        for (const auto &frame : m_frames_sfx)
            frameSfxArray.append(frame.toJson());
        json["frames_sfx"] = frameSfxArray;
    }

    json["additive"] = m_additive;
    json["effect"] = m_effect;
    json["blips"] = m_blips;

    return json;
}

bool ms2::FrameData::fromJson(const QJsonObject &f_json, FrameData &f_data)
{
    if (const QJsonValue v = f_json["emote"]; v.isString())
        f_data.m_emote = v.toString();
    else
        return false;

    if (const QJsonValue v = f_json["frame"]; v.isDouble())
        f_data.m_frame = v.toInt();
    else
        return false;

    if (const QJsonValue v = f_json["value"]; v.isString())
        f_data.m_value = v.toString();

    return true;
}

QJsonObject ms2::FrameData::toJson() const
{
    QJsonObject json;

    json["emote"] = m_emote;
    json["frame"] = m_frame;

    if (!m_value.isEmpty()) {
        json["value"] = m_value;
    }

    return json;
}
