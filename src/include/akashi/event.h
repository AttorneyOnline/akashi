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

struct PlayerJoinedAreaEvent : Event
{
    Q_GADGET_EXPORT(AKASHI_CORE_EXPORT)
    Q_PROPERTY(int client_id MEMBER client_id)
    Q_PROPERTY(int area_id MEMBER area_id)
    Q_PROPERTY(int floor_id MEMBER floor_id)
    Q_PROPERTY(QString char_name MEMBER char_name)
    Q_PROPERTY(QString ipid MEMBER ipid)

  public:
    static inline const QString id = QStringLiteral("player_joined");

    int client_id = -1;
    int area_id = -1;
    int floor_id = -1;
    QString char_name;
    QString ipid;
};

struct PlayerLeftAreaEvent : Event
{
    Q_GADGET_EXPORT(AKASHI_CORE_EXPORT)
    Q_PROPERTY(int client_id MEMBER client_id)
    Q_PROPERTY(int area_id MEMBER area_id)
    Q_PROPERTY(int floor_id MEMBER floor_id)
    Q_PROPERTY(QString char_name MEMBER char_name)

  public:
    static inline const QString id = QStringLiteral("player_left");

    int client_id = -1;
    int area_id = -1;
    int floor_id = -1;
    QString char_name;
};

struct AreaChangedEvent : Event
{
    Q_GADGET_EXPORT(AKASHI_CORE_EXPORT)
    Q_PROPERTY(int client_id MEMBER client_id)
    Q_PROPERTY(int from_area MEMBER from_area)
    Q_PROPERTY(int to_area MEMBER to_area)
    Q_PROPERTY(QString char_name MEMBER char_name)

  public:
    static inline const QString id = QStringLiteral("area_changed");

    int client_id = -1;
    int from_area = -1;
    int to_area = -1;
    QString char_name;
};

struct ICMessageEvent : Event
{
    Q_GADGET_EXPORT(AKASHI_CORE_EXPORT)
    Q_PROPERTY(int client_id MEMBER client_id)
    Q_PROPERTY(int area_id MEMBER area_id)
    Q_PROPERTY(int floor_id MEMBER floor_id)
    Q_PROPERTY(QString area_name MEMBER area_name)
    Q_PROPERTY(QString char_name MEMBER char_name)
    Q_PROPERTY(QString ooc_name MEMBER ooc_name)
    Q_PROPERTY(QString ipid MEMBER ipid)
    Q_PROPERTY(QString message MEMBER message)

  public:
    static inline const QString id = QStringLiteral("ic_message_sent");

    int client_id = -1;
    int area_id = -1;
    int floor_id = -1;
    QString area_name;
    QString char_name;
    QString ooc_name;
    QString ipid;
    QString message;
};

struct OOCMessageEvent : Event
{
    Q_GADGET_EXPORT(AKASHI_CORE_EXPORT)
    Q_PROPERTY(int client_id MEMBER client_id)
    Q_PROPERTY(int area_id MEMBER area_id)
    Q_PROPERTY(QString area_name MEMBER area_name)
    Q_PROPERTY(QString char_name MEMBER char_name)
    Q_PROPERTY(QString ooc_name MEMBER ooc_name)
    Q_PROPERTY(QString ipid MEMBER ipid)
    Q_PROPERTY(QString message MEMBER message)

  public:
    static inline const QString id = QStringLiteral("ooc_message_sent");

    int client_id = -1;
    int area_id = -1;
    QString area_name;
    QString char_name;
    QString ooc_name;
    QString ipid;
    QString message;
};

struct MusicChangedEvent : Event
{
    Q_GADGET_EXPORT(AKASHI_CORE_EXPORT)
    Q_PROPERTY(int client_id MEMBER client_id)
    Q_PROPERTY(int area_id MEMBER area_id)
    Q_PROPERTY(int floor_id MEMBER floor_id)
    Q_PROPERTY(QString area_name MEMBER area_name)
    Q_PROPERTY(QString char_name MEMBER char_name)
    Q_PROPERTY(QString track_name MEMBER track_name)

  public:
    static inline const QString id = QStringLiteral("music_changed");

    int client_id = -1;
    int area_id = -1;
    int floor_id = -1;
    QString area_name;
    QString char_name;
    QString track_name;
};

struct EvidencePresentedEvent : Event
{
    Q_GADGET_EXPORT(AKASHI_CORE_EXPORT)
    Q_PROPERTY(int client_id MEMBER client_id)
    Q_PROPERTY(int area_id MEMBER area_id)
    Q_PROPERTY(int floor_id MEMBER floor_id)
    Q_PROPERTY(QString char_name MEMBER char_name)
    Q_PROPERTY(QString evidence_name MEMBER evidence_name)

  public:
    static inline const QString id = QStringLiteral("evidence_presented");

    int client_id = -1;
    int area_id = -1;
    int floor_id = -1;
    QString char_name;
    QString evidence_name;
};

struct ModcallEvent : Event
{
    Q_GADGET_EXPORT(AKASHI_CORE_EXPORT)
    Q_PROPERTY(int client_id MEMBER client_id)
    Q_PROPERTY(int area_id MEMBER area_id)
    Q_PROPERTY(QString area_name MEMBER area_name)
    Q_PROPERTY(QString char_name MEMBER char_name)
    Q_PROPERTY(QString ooc_name MEMBER ooc_name)
    Q_PROPERTY(QString ipid MEMBER ipid)
    Q_PROPERTY(QString reason MEMBER reason)

  public:
    static inline const QString id = QStringLiteral("modcall");

    int client_id = -1;
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
    Q_PROPERTY(int client_id MEMBER client_id)
    Q_PROPERTY(bool was_joined MEMBER was_joined)
    Q_PROPERTY(QString ipid MEMBER ipid)
    Q_PROPERTY(QString hwid MEMBER hwid)
    Q_PROPERTY(QString char_name MEMBER char_name)
    Q_PROPERTY(QString ooc_name MEMBER ooc_name)

  public:
    static inline const QString id = QStringLiteral("player_disconnected");

    int client_id = -1;
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

Q_DECLARE_METATYPE(akashi::PlayerJoinedAreaEvent)
Q_DECLARE_METATYPE(akashi::PlayerLeftAreaEvent)
Q_DECLARE_METATYPE(akashi::AreaChangedEvent)
Q_DECLARE_METATYPE(akashi::ICMessageEvent)
Q_DECLARE_METATYPE(akashi::OOCMessageEvent)
Q_DECLARE_METATYPE(akashi::MusicChangedEvent)
Q_DECLARE_METATYPE(akashi::EvidencePresentedEvent)
Q_DECLARE_METATYPE(akashi::ModcallEvent)
Q_DECLARE_METATYPE(akashi::BanIssuedEvent)
Q_DECLARE_METATYPE(akashi::KickIssuedEvent)
Q_DECLARE_METATYPE(akashi::ConfigReloadedEvent)
