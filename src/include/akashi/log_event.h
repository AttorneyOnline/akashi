#pragma once

#include <QString>

namespace akashi {

namespace log_type {
inline const QString IC = QStringLiteral("ic");
inline const QString OOC = QStringLiteral("ooc");
inline const QString Music = QStringLiteral("music");
inline const QString Login = QStringLiteral("login");
inline const QString CMD = QStringLiteral("cmd");
inline const QString Kick = QStringLiteral("kick");
inline const QString Ban = QStringLiteral("ban");
inline const QString Modcall = QStringLiteral("modcall");
inline const QString Connect = QStringLiteral("connect");
} // namespace log_type

struct LogEvent
{
    qint64 timestamp = 0;
    QString type;
    QString area;
    QString char_name;
    QString ooc_name;
    QString ipid;
    QString client_id;
    QString message;
    QString args;
    QString moderator;
    QString target_ipid;
    QString duration;
    QString hwid;
    bool success = true;
};

} // namespace akashi

