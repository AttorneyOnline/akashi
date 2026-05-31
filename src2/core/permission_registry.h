#pragma once

#include "akashi/service.h"
#include "akashi_core_export.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringLiteral>

#include <functional>
#include <optional>

class QThread;

namespace akashi {

struct PermissionInfo
{
    QString id;
    QString display_name;
    QString description;
    QString category;
};

struct PermissionQuery
{
    QString permission;
    int client_id = -1;
    int area_id = -1;
    bool is_authenticated = false;
    QString auth_type;
    QString acl_role_id;
};

enum class PermissionVerdict
{
    Granted,
    Denied,
    NoOpinion,
};

using PermissionResolver = std::function<PermissionVerdict(const PermissionQuery &)>;

class AKASHI_CORE_EXPORT PermissionRegistry : public IService
{
  public:
    PermissionRegistry();

    QString serviceId() const override;
    ServiceVersion serviceVersion() const override;

    bool registerPermission(const PermissionInfo &f_info, const QString &f_owner_id = {});
    void unregisterAllPermissions(const QString &f_owner_id);
    bool isRegistered(const QString &f_permission_id) const;

    QList<PermissionInfo> permissions() const;
    QList<PermissionInfo> permissionsByCategory(const QString &f_category) const;
    std::optional<PermissionInfo> permissionById(const QString &f_permission_id) const;

    bool registerResolver(const QString &f_resolver_id, int f_priority,
                          PermissionResolver f_resolver, const QString &f_owner_id = {});
    void unregisterAllResolvers(const QString &f_owner_id);
    bool resolve(const PermissionQuery &f_query) const;

  private:
    struct PermissionEntry
    {
        PermissionInfo info;
        QString owner_id;
    };

    struct ResolverEntry
    {
        QString resolver_id;
        int priority = 0;
        PermissionResolver resolver;
        QString owner_id;
    };

    QHash<QString, PermissionEntry> m_permissions;
    QList<ResolverEntry> m_resolvers;
    QThread *m_owner_thread;
};

namespace permission {
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
    inline const QString super = QStringLiteral("super");
} // namespace permission

namespace sanction {
    inline const QString muted = QStringLiteral("muted");
    inline const QString ooc_muted = QStringLiteral("ooc_muted");
    inline const QString dj_blocked = QStringLiteral("dj_blocked");
    inline const QString wtce_blocked = QStringLiteral("wtce_blocked");
    inline const QString gimped = QStringLiteral("gimped");
    inline const QString disemvoweled = QStringLiteral("disemvoweled");
    inline const QString shaken = QStringLiteral("shaken");
    inline const QString medieval = QStringLiteral("medieval");
} // namespace sanction

} // namespace akashi

