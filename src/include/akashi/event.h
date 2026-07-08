#pragma once

#include <QMetaType>
#include <QString>
#include <QVariantMap>

namespace akashi {

struct Event
{
    bool cancelled = false;
    QString cancel_reason;
    void cancel(const QString &f_reason = {})
    {
        cancelled = true;
        cancel_reason = f_reason;
    }
};

enum class EventPhase
{
    Before,
    After,
};

struct PlayerJoinedAreaEvent : Event
{
    int client_id = -1;
    int area_id = -1;
    int floor_id = -1;
    QString char_name;
    QString ipid;
};

struct PlayerLeftAreaEvent : Event
{
    int client_id = -1;
    int area_id = -1;
    int floor_id = -1;
    QString char_name;
};

struct AreaChangedEvent : Event
{
    int client_id = -1;
    int from_area = -1;
    int to_area = -1;
    QString char_name;
};

struct ICMessageEvent : Event
{
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
    int client_id = -1;
    int area_id = -1;
    int floor_id = -1;
    QString area_name;
    QString char_name;
    QString track_name;
};

struct EvidencePresentedEvent : Event
{
    int client_id = -1;
    int area_id = -1;
    int floor_id = -1;
    QString char_name;
    QString evidence_name;
};

struct ModcallEvent : Event
{
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
    int ban_id = -1;
    QString moderator;
    QString target_ipid;
    QString duration;
    QString reason;
};

struct KickIssuedEvent : Event
{
    QString moderator;
    QString target_ipid;
    QString reason;
};

struct CommandExecutedEvent : Event
{
    int client_id = -1;
    int area_id = -1;
    QString char_name;
    QString ipid;
    QString command;
    QString args;
};

struct ConfigReloadedEvent : Event
{
};

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
Q_DECLARE_METATYPE(akashi::CommandExecutedEvent)
Q_DECLARE_METATYPE(akashi::ConfigReloadedEvent)
