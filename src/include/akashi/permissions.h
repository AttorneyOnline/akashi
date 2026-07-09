#pragma once

#include <QString>

// The permission ids core declares. Commands gate on them, role files
// grant them, and rule arguments name them. Plugins declare their own ids
// through the PermissionRegistry service and reference them as plain
// strings; these constants only exist so core never mistypes its own.
namespace akashi::permission {
inline const QString none = QStringLiteral("none");
inline const QString kick = QStringLiteral("kick");
inline const QString ban = QStringLiteral("ban");
inline const QString lock_background = QStringLiteral("lock_background");
inline const QString modify_users = QStringLiteral("modify_users");
inline const QString gamemaster = QStringLiteral("gamemaster");
inline const QString global_timer = QStringLiteral("global_timer");
inline const QString modify_evidence = QStringLiteral("modify_evidence");
inline const QString motd = QStringLiteral("motd");
inline const QString announcer = QStringLiteral("announcer");
inline const QString chat_moderator = QStringLiteral("chat_moderator");
inline const QString mute = QStringLiteral("mute");
inline const QString remove_gamemaster = QStringLiteral("remove_gamemaster");
inline const QString save_testimony = QStringLiteral("save_testimony");
inline const QString force_charselect = QStringLiteral("force_charselect");
inline const QString bypass_locks = QStringLiteral("bypass_locks");
inline const QString ignore_background_list = QStringLiteral("ignore_background_list");
inline const QString send_notice = QStringLiteral("send_notice");
inline const QString jukebox = QStringLiteral("jukebox");
inline const QString modify_rules = QStringLiteral("modify_rules");
inline const QString modify_floors = QStringLiteral("modify_floors");
inline const QString super = QStringLiteral("super");
inline const QString user = QStringLiteral("user");
inline const QString bypass_rules = QStringLiteral("bypass_rules");
} // namespace akashi::permission
