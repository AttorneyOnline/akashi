#pragma once

#include "akashi/permissions.h"
#include "akashi/sanctions.h"
#include "akashi/service.h"
#include "akashi_core_export.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringLiteral>

#include <functional>
#include <optional>

class QSettings;
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

// One named set of permission ids, as granted to a moderator account.
// "super" is a wildcard that grants everything.
class AKASHI_CORE_EXPORT ACLRole
{
  public:
    ACLRole();
    explicit ACLRole(const QSet<QString> &f_permissions);

    // True when the permission is held, empty or "none" is asked for, or
    // the role holds "super".
    bool canPerform(const QString &f_permission) const;

    void setPermission(const QString &f_permission, bool f_mode);

    const QSet<QString> &permissions() const;

  private:
    QSet<QString> m_permissions;
};

// The named roles moderator accounts point at, loaded from and saved to the
// server owner's roles file. NONE and SUPER always exist and are read-only.
class AKASHI_CORE_EXPORT ACLRolesHandler : public QObject
{
    Q_OBJECT

  public:
    static const QString NONE_ID;
    static const QString SUPER_ID;

    explicit ACLRolesHandler(QObject *parent = nullptr);
    ~ACLRolesHandler();

    // Role identifiers are not case-sensitive.
    bool roleExists(QString f_id);

    // The role with the given identifier, or a permissionless role when it
    // does not exist.
    ACLRole roleById(QString f_id);

    // Inserts or overwrites a role. Read-only roles cannot be replaced.
    bool insertRole(QString f_id, ACLRole f_role);

    // Read-only roles cannot be removed.
    bool removeRole(QString f_id);

    void clearRoles();

    // Replaces the current roles with the file's. Every key in a role's
    // section names a permission id, so plugin permissions round-trip.
    bool loadFile(QString f_filename);

    // Saves the current roles, completely overwriting the file.
    bool saveFile(QString f_filename);

  private:
    static bool checkSettingsStatus(QSettings *f_settings);

    // Shared read-only standard roles.
    static const QHash<QString, ACLRole> readonly_roles;

    QHash<QString, ACLRole> m_roles;
};

} // namespace akashi
