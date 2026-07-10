#pragma once

#include "akashi_core_export.h"

#include <QMetaProperty>
#include <QMetaType>
#include <QObject>
#include <QString>
#include <QVariantMap>

namespace akashi {

// Events are plain value types. Each struct is a Q_GADGET so the same
// definition serves typed C++ access and, via eventToMap, the key/value
// payload rules and scripts see. The id names the event; where the
// concept maps to a rule catalog event the strings match. Gating lives
// in the rule system's before phase; an event struct is a committed fact.
struct Event
{
    Q_GADGET_EXPORT(AKASHI_CORE_EXPORT)
};

// Placed events dispatch as QVariantMap payloads through the rule-phase
// wrappers, never as typed structs; the markers below survive as the id
// constants, with each event's real observer payload documented. Every
// player-driven placed payload additionally carries the stamped actor
// keys - client_session_id (the session), player_state_id (the acting
// user slot), char_name (the character folder), ooc_name - unless the
// firing site provided the key itself. Events fired before a character
// exists (like server_joined) stamp empty names.

// Observer payload: from_area, from_floor, character_taken + actor keys.
// The before phase instead carries lock_status, is_invited, bypass_locks,
// area_name, character_id + actor keys.
struct PlayerJoinedAreaEvent
{
    static inline const QString id = QStringLiteral("player_joined");
};

// Observer payload: the actor keys only.
struct PlayerLeftAreaEvent
{
    static inline const QString id = QStringLiteral("player_left");
};

// Reserved id: nothing fires this event today.
struct AreaChangedEvent
{
    static inline const QString id = QStringLiteral("area_changed");
};

// Observer payload: message + actor keys; char_name is the wire value the
// packet carried. The before/transform phases additionally see
// objection_mod, showname, evidence.
struct ICMessageEvent
{
    static inline const QString id = QStringLiteral("ic_message_sent");
};

// Observer payload: message + actor keys.
struct OOCMessageEvent
{
    static inline const QString id = QStringLiteral("ooc_message_sent");
};

// Observer payload: song, source + actor keys. The jukebox fires it with
// no actor keys and both context ids -1.
struct MusicChangedEvent
{
    static inline const QString id = QStringLiteral("music_changed");
};

// Observer payload: index + actor keys.
struct EvidencePresentedEvent
{
    static inline const QString id = QStringLiteral("evidence_presented");
};

struct ModcallEvent : Event
{
    Q_GADGET_EXPORT(AKASHI_CORE_EXPORT)
    Q_PROPERTY(int client_session_id MEMBER client_session_id)
    Q_PROPERTY(int player_state_id MEMBER player_state_id)
    Q_PROPERTY(int area_id MEMBER area_id)
    Q_PROPERTY(QString area_name MEMBER area_name)
    Q_PROPERTY(QString char_name MEMBER char_name)
    Q_PROPERTY(QString ooc_name MEMBER ooc_name)
    Q_PROPERTY(QString ipid MEMBER ipid)
    Q_PROPERTY(QString reason MEMBER reason)

  public:
    static inline const QString id = QStringLiteral("modcall");

    int client_session_id = -1;
    // The user slot that filed the call; equals client_session_id while a
    // session holds one slot.
    int player_state_id = -1;
    int area_id = -1;
    QString area_name;
    QString char_name;
    QString ooc_name;
    QString ipid;
    QString reason;
};

struct BanIssuedEvent : Event
{
    Q_GADGET_EXPORT(AKASHI_CORE_EXPORT)
    Q_PROPERTY(int ban_id MEMBER ban_id)
    Q_PROPERTY(QString moderator MEMBER moderator)
    Q_PROPERTY(QString target_ipid MEMBER target_ipid)
    Q_PROPERTY(QString duration MEMBER duration)
    Q_PROPERTY(QString reason MEMBER reason)

  public:
    static inline const QString id = QStringLiteral("ban_issued");

    int ban_id = -1;
    QString moderator;
    QString target_ipid;
    QString duration;
    QString reason;
};

struct KickIssuedEvent : Event
{
    Q_GADGET_EXPORT(AKASHI_CORE_EXPORT)
    Q_PROPERTY(QString moderator MEMBER moderator)
    Q_PROPERTY(QString target_ipid MEMBER target_ipid)
    Q_PROPERTY(QString reason MEMBER reason)

  public:
    static inline const QString id = QStringLiteral("kick_issued");

    QString moderator;
    QString target_ipid;
    QString reason;
};

// Fires when a client is finally removed - after any reconnect grace, so
// plugins can safely clean up whatever they keyed on the player.
struct PlayerDisconnectedEvent : Event
{
    Q_GADGET_EXPORT(AKASHI_CORE_EXPORT)
    Q_PROPERTY(int client_session_id MEMBER client_session_id)
    Q_PROPERTY(int player_state_id MEMBER player_state_id)
    Q_PROPERTY(bool was_joined MEMBER was_joined)
    Q_PROPERTY(QString ipid MEMBER ipid)
    Q_PROPERTY(QString hwid MEMBER hwid)
    Q_PROPERTY(QString char_name MEMBER char_name)
    Q_PROPERTY(QString ooc_name MEMBER ooc_name)

  public:
    static inline const QString id = QStringLiteral("player_disconnected");

    int client_session_id = -1;
    // The session's active user slot at removal time.
    int player_state_id = -1;
    bool was_joined = false;
    QString ipid;
    QString hwid;
    QString char_name;
    QString ooc_name;
};

struct ConfigReloadedEvent : Event
{
    Q_GADGET_EXPORT(AKASHI_CORE_EXPORT)

  public:
    static inline const QString id = QStringLiteral("config_reloaded");
};

// The bridge between typed events and the rules' map world: every
// declared property lands in the map under its own name.
template <typename T>
QVariantMap eventToMap(const T &f_event)
{
    QVariantMap l_map;
    const QMetaObject &l_meta = T::staticMetaObject;
    for (int i = 0; i < l_meta.propertyCount(); ++i) {
        const QMetaProperty l_property = l_meta.property(i);
        l_map.insert(QString::fromLatin1(l_property.name()), l_property.readOnGadget(&f_event));
    }
    return l_map;
}

} // namespace akashi

Q_DECLARE_METATYPE(akashi::ModcallEvent)
Q_DECLARE_METATYPE(akashi::BanIssuedEvent)
Q_DECLARE_METATYPE(akashi::KickIssuedEvent)
Q_DECLARE_METATYPE(akashi::ConfigReloadedEvent)
