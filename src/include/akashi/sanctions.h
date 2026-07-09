#pragma once

#include <QString>

// The sanction ids core applies. A sanction is a flag in a client's
// sanction set; its effect lives wherever that flag is read (a text
// filter, a gate). Plugin sanctions are plain strings of their own.
namespace akashi::sanction {
inline const QString muted = QStringLiteral("muted");
inline const QString ooc_muted = QStringLiteral("ooc_muted");
inline const QString dj_blocked = QStringLiteral("dj_blocked");
inline const QString wtce_blocked = QStringLiteral("wtce_blocked");
inline const QString gimped = QStringLiteral("gimped");
inline const QString disemvoweled = QStringLiteral("disemvoweled");
inline const QString shaken = QStringLiteral("shaken");
inline const QString medieval = QStringLiteral("medieval");
// Restricts character choice to a moderator-picked list.
inline const QString charcurse = QStringLiteral("charcurse");
} // namespace akashi::sanction
