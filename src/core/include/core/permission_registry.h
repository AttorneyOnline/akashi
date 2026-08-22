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
    // Restricted acts never ride an everyone-shaped offer, whatever the
    // scope. Checked at every door a grant can come through, so a plugin
    // may mark its own act a role grant too.
    bool restricted = false;
};

struct PermissionQuery
{
    QString permission;
    int client_id = -1;
    int area_id = -1;
    int floor_id = -1; // the floor the area sits on, for floor-scope offers
    bool is_authenticated = false;
    QString auth_type;
    QString acl_role_id;
    QString ipid;            // the person key for person-audience grants
    bool is_joined = false;  // everyone-audience covers joined sessions only
    QSet<QString> sanctions; // the actor's active sanctions, for the mask stage
};

// A live fact the world knows and a grant cannot express: the area's
// lock, a plugin's own condition. A source agrees or it does not - it
// never names a permission, so it cannot answer a question it was not
// asked. Sources only ever add; there is no deny form.
using GrantSource = std::function<bool(const PermissionQuery &)>;

// The roles an actor wears here: the one they authenticated into, plus
// any the world's live state confers - owning an area is wearing @cm in
// it. Worn roles both hold permissions of their own and satisfy
// role-audience grants.
using RoleProvider = std::function<QStringList(const PermissionQuery &)>;

// One contributor that answered a query, for /why. A stored offer
// answers with itself; a role and a live fact answer with the name and
// the words they were registered under.
struct Contribution
{
    enum class Kind
    {
        Grant,
        Role,
        Source,
    };

    Kind kind = Kind::Grant;
    QString id;                 // the role id, or the source id
    QString because;            // the words a source was registered with
    std::optional<Grant> grant; // set when a stored offer answered
};

// A resolution taken apart for introspection: the mask outranks
// everything, so masked_by non-empty means refused whatever else agreed.
struct Resolution
{
    bool allowed = false;
    QList<Contribution> contributions;
    QStringList masked_by;
};

// One offer as an operator row: what it grants, to whom, where it stands
// and who put it there. The --check-config dump and /permissions share
// it, so the two can never describe the same offer differently. Pass the
// place's name when the caller can resolve it; otherwise the id prints.
AKASHI_CORE_EXPORT QString describeGrant(const Grant &f_grant, const QString &f_place_name = {});

// Resolution is a fixed pipeline: the sanction mask - the model's one
// subtraction - and then the additive union of worn roles, standing
// grants and live facts. Everything else (target immunity, escalations,
// AND-groups) is dispatch predicate work that lives at the gates.
class AKASHI_CORE_EXPORT PermissionRegistry : public IService
{
  public:
    PermissionRegistry();

    QString serviceId() const override;
    ServiceVersion serviceVersion() const override;

    bool registerPermission(const PermissionInfo &f_info, const QString &f_owner_id = {});
    // Drops the owner's permissions and every grant that named one of
    // them, so an unloaded plugin cannot leave an offer standing against
    // a catalog entry that no longer exists. Answers with the ids it
    // dropped.
    QStringList unregisterAllPermissions(const QString &f_owner_id);
    bool isRegistered(const QString &f_permission_id) const;

    QList<PermissionInfo> permissions() const;
    QList<PermissionInfo> permissionsByCategory(const QString &f_category) const;
    std::optional<PermissionInfo> permissionById(const QString &f_permission_id) const;
    QStringList permissionsOwnedBy(const QString &f_owner_id) const;

    // The one gate every standing offer passes: the permission must be
    // registered (the rose-garden rule), and a restricted act may not
    // ride an everyone-shaped audience. Callers that store an offer
    // elsewhere ask this first; addGrant asks it for them.
    std::optional<Grant> admit(const Grant &f_grant, const QString &f_where) const;

    // Grant storage for every scope. A floor or area offer names its
    // place through the grant's place_id.
    bool addGrant(const Grant &f_grant);
    // Removes exactly the matching entry; false when no such grant stands.
    bool removeGrant(const Grant &f_grant);
    void removeGrantsByOwner(const QString &f_owner_id);
    // Every stored offer, sorted, so the --check-config dump diffs
    // cleanly between runs.
    QList<Grant> grants() const;

    // The roles an actor wears. One provider, installed by the host;
    // without one, only the authenticated role counts.
    void setRoleProvider(RoleProvider f_provider);
    QStringList rolesWorn(const PermissionQuery &f_query) const;

    // Whether a worn role holds a permission. The host installs this
    // alongside the provider, since the roles file is its to read.
    using RoleHoldsFn = std::function<bool(const QString &f_role_id, const QString &f_permission)>;
    void setRoleHolds(RoleHoldsFn f_role_holds);

    // A live fact seam. f_because is the words /why prints; f_interests
    // names the permissions the source can answer, and empty means any.
    bool registerGrantSource(const QString &f_source_id, const QString &f_because, GrantSource f_source,
                             const QString &f_owner_id = {}, const QSet<QString> &f_interests = {});
    void unregisterAllGrantSources(const QString &f_owner_id);
    // Replaces one source's interest set - the case-manager bundle moves
    // when the roles file reloads.
    bool setSourceInterests(const QString &f_source_id, const QSet<QString> &f_interests);

    // A sanction mask suppresses a permission while the sanction is on
    // the actor - audience-blind: it beats every grant, super's included.
    // One sanction may mask several acts.
    bool registerSanctionMask(const QString &f_sanction_id, const QString &f_permission_id, const QString &f_owner_id = {});
    void unregisterAllSanctionMasks(const QString &f_owner_id);

    // The pipeline's boolean face: it leaves the moment it knows.
    bool resolve(const PermissionQuery &f_query) const;

    // The same pipeline recording every contribution, in fixed order
    // (roles, then grants by scope tier, then live facts) so the answer
    // prints identically run to run.
    Resolution resolveExplained(const PermissionQuery &f_query) const;

    // True when the audience covers the query's actor. Everyone covers
    // joined sessions; a role is one the actor wears here; a person is
    // their IPID. Participants is defined with the spectator work.
    static bool audienceCovers(const Audience &f_audience, const PermissionQuery &f_query,
                               const QStringList &f_worn_roles);

    // True when the offer's scope covers the place the query asks about.
    static bool scopeCovers(const Grant &f_grant, const PermissionQuery &f_query);

  private:
    struct PermissionEntry
    {
        PermissionInfo info;
        QString owner_id;
    };

    struct SourceEntry
    {
        QString source_id;
        QString because;
        GrantSource source;
        QString owner_id;
        QSet<QString> interests;

        bool answers(const QString &f_permission) const
        {
            return interests.isEmpty() || interests.contains(f_permission);
        }
    };

    struct MaskEntry
    {
        QString permission_id;
        QString owner_id;
    };

    enum class Mode
    {
        StopAtFirst,
        CollectAll,
    };

    Resolution resolveInternal(const PermissionQuery &f_query, Mode f_mode) const;
    void removeGrantsNaming(const QSet<QString> &f_permissions);

    QHash<QString, PermissionEntry> m_permissions;
    QHash<QString, QList<Grant>> m_grants;
    QList<SourceEntry> m_sources;
    QHash<QString, QList<MaskEntry>> m_sanction_masks;
    RoleProvider m_role_provider;
    RoleHoldsFn m_role_holds;
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
