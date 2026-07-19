#pragma once

#include "akashi/grants.h"
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
    QString ipid;            // the person key for person-audience grants
    bool is_joined = false;  // everyone-audience covers joined sessions only
    QSet<QString> sanctions; // the actor's active sanctions, for the mask stage
};

// A grant source contributes standing offers to the union: the grants it
// returns must already match the query's permission and cover its actor.
// A source can only ever add - there is no deny form.
using GrantSource = std::function<QList<Grant>(const PermissionQuery &)>;

// One grant that answered a query, with the source that offered it.
struct GrantMatch
{
    Grant grant;
    QString source_id;
};

// A resolution taken apart for introspection: the mask outranks any
// grant, so masked_by non-empty means refused whatever matched.
struct Resolution
{
    bool allowed = false;
    QList<GrantMatch> matched;
    QStringList masked_by;
};

// Resolution is a fixed two-stage pipeline: the additive grant union,
// then the sanction mask - the model's one subtraction. Everything else
// (target immunity, escalations, AND-groups) is dispatch predicate work
// that lives at the gates, never in here.
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

    // Server-scope grant storage - the entries the config compile and
    // runtime grants write. The permission name must be registered;
    // unknown names are refused loudly (the rose-garden rule).
    bool addGrant(const Grant &f_grant);
    // Removes exactly the matching entry; false when no such grant stands.
    bool removeGrant(const Grant &f_grant);
    void removeGrantsByOwner(const QString &f_owner_id);
    QList<Grant> serverGrants() const;

    // Grant sources for offers that live outside this registry: role
    // files, area ownership, floor and area offers, live state.
    bool registerGrantSource(const QString &f_source_id, GrantSource f_source, const QString &f_owner_id = {});
    void unregisterAllGrantSources(const QString &f_owner_id);

    // A sanction mask suppresses one permission while the sanction is on
    // the actor - audience-blind: it beats every grant, super's included.
    bool registerSanctionMask(const QString &f_sanction_id, const QString &f_permission_id, const QString &f_owner_id = {});
    void unregisterAllSanctionMasks(const QString &f_owner_id);

    // The pipeline's boolean face: mask first (early exit), then the
    // union walked until any source offers.
    bool resolve(const PermissionQuery &f_query) const;

    // The same pipeline recording every contribution, in fixed tier order
    // (server, floor, area, then person-audience entries; stable by owner
    // then permission) so the answer prints identically run to run.
    Resolution resolveExplained(const PermissionQuery &f_query) const;

    // True when the audience covers the query's actor. Everyone covers
    // joined sessions; a role is worn through authentication; a person is
    // their IPID. Participants is defined with the spectator work.
    static bool audienceCovers(const Audience &f_audience, const PermissionQuery &f_query);

  private:
    struct PermissionEntry
    {
        PermissionInfo info;
        QString owner_id;
    };

    struct SourceEntry
    {
        QString source_id;
        GrantSource source;
        QString owner_id;
    };

    struct MaskEntry
    {
        QString permission_id;
        QString owner_id;
    };

    bool isMasked(const PermissionQuery &f_query) const;

    QHash<QString, PermissionEntry> m_permissions;
    QList<Grant> m_server_grants;
    QList<SourceEntry> m_sources;
    QHash<QString, MaskEntry> m_sanction_masks;
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
// server owner's roles file (permissions.json). NONE and SUPER always exist
// and are read-only. The file may carry a "groups" section defining named,
// nestable permission sets referenced as "@name" from roles and offers -
// the sigil marks a keyring, and a typo like @moderaton fails loudly as an
// unknown group instead of loading as a mystery permission.
class AKASHI_CORE_EXPORT ACLRolesHandler : public QObject
{
    Q_OBJECT

  public:
    static const QString NONE_ID;
    static const QString SUPER_ID;

    explicit ACLRolesHandler(QObject *parent = nullptr);
    ~ACLRolesHandler();

    // Role identifiers are not case-sensitive.
    bool roleExists(const QString &f_id) const;

    // The role with the given identifier, or a permissionless role when it
    // does not exist.
    ACLRole roleById(const QString &f_id) const;

    // Inserts or overwrites a role. Read-only roles cannot be replaced.
    bool insertRole(QString f_id, ACLRole f_role);

    // Read-only roles cannot be removed.
    bool removeRole(QString f_id);

    void clearRoles();

    // Group lookups take the reference with or without its @ sigil,
    // case-insensitively, and answer with the expanded leaf permissions.
    bool groupExists(const QString &f_reference) const;
    QStringList groupMembers(const QString &f_reference) const;

    // Every loaded role and group name, sorted - the --check-config dump
    // walks these so its output diffs cleanly between builds.
    QStringList roleIds() const;
    QStringList groupNames() const;

    // The reserved everyone section: the config-defined baseline of
    // ordinary operations every joined person holds, expanded through
    // groups and aliases like a role body. Empty when the file has no
    // section - the caller applies the stock default then.
    std::optional<QStringList> everyoneBaseline() const;

    // Replaces the current roles and groups with the file's. Every key in
    // a role's section names a permission id (so plugin permissions
    // round-trip) or an @group reference expanded from the groups section.
    bool loadFile(QString f_filename);

    // Saves the current roles, completely overwriting the file.
    bool saveFile(QString f_filename);

  private:
    static bool checkSettingsStatus(QSettings *f_settings);
    static QString groupKey(const QString &f_reference);

    // Shared read-only standard roles.
    static const QHash<QString, ACLRole> readonly_roles;

    QHash<QString, ACLRole> m_roles;
    // Group name (lowercase, no sigil) to its expanded leaf permissions.
    QHash<QString, QStringList> m_groups;
    // The everyone section's expanded members, if the file declared one.
    std::optional<QStringList> m_everyone;
};

} // namespace akashi
