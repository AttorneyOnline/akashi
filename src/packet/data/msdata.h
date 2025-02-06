#pragma once

#include <QJsonObject>
#include <QList>

namespace ms2 {
enum class DeskMod
{
    Shown = 1,
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

    /// Returns true if the minimal error-checking it does reported no issues when reading from JSON.
    static bool fromJson(const QJsonObject &f_json, FrameData &f_data);
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
    qint32 m_self_x_offset;
    qint32 m_self_y_offset;
    qint32 m_other_x_offset;
    qint32 m_other_y_offset;
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

    /// Returns true if the minimal error-checking it does reported no issues when reading from JSON.
    static bool fromJson(const QJsonObject &f_json, OldMSFlatData &f_data);
    QJsonObject toJson() const;
};
}
