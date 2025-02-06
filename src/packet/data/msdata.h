#pragma once

#include <QJsonObject>
#include <QList>

namespace ms2 {
enum class DeskMod
{
    Hidden = 0,
    Shown,
    HiddenDuringPreThenShown,
    ShowDuringPreThenHidden,
    HiddenCentreThenShown,
    ShownThenHiddenCentre,
};

enum class EmoteMod
{
    NoPre = 0,
    Pre,
    PreAndObject,
    NoPreZoom,
    NoPreObjZoom,
};

enum class ObjectionMod
{
    None = 0,
    HoldIt,
    Objection,
    TakeThat,
    Custom,
};

struct FrameData
{
    QString m_emote;
    qint32 m_frame;
    QString m_value;

    static bool fromJson(const QJsonObject &f_json, FrameData &f_data);
    QJsonObject toJson() const;
};

struct OffsetData {
    qint32 x;
    qint32 y;

    static bool fromJson(const QJsonObject &f_json, OffsetData &f_data);
    QJsonObject toJson() const;
};

/// This is a super-basic representation of the old MS packet in the flattest JSON form possible.
/// Intentionally minimal types, almost no error checking.
struct OldMSFlatData
{
    DeskMod m_desk_mod;
    QString m_preanim;
    QString m_char_name;
    QString m_emote;
    QString m_message_text;
    QString m_side;
    QString m_sfx_name;
    EmoteMod m_emote_mod;
    qint32 m_char_id;
    qint32 m_sfx_delay;
    ObjectionMod m_objection_mod;
    QString m_objection_custom;
    qint32 m_evidence;
    bool m_flip;
    bool m_realisation;
    qint32 m_text_colour;
    QString m_showname;
    qint32 m_other_charid;
    QString m_other_name;
    QString m_other_emote;
    OffsetData m_self_offset;
    OffsetData m_other_offset;
    bool m_other_flip;
    bool m_immediate;
    bool m_sfx_looping;
    bool m_screenshake;
    QList<FrameData> m_frames_shake;
    QList<FrameData> m_frames_realisation;
    QList<FrameData> m_frames_sfx;
    bool m_additive;
    QString m_effect;
    QString m_blips;
    bool m_slide;

    /// Returns true if the minimal error-checking it does reported no issues when reading from JSON.
    static bool fromJson(const QJsonObject &f_json, OldMSFlatData &f_data);
    QJsonObject toJson() const;
};
}
