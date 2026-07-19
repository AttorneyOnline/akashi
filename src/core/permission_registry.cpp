#include "core/permission_registry.h"

#include "akashi/logging_categories.h"
#include "akashi/thread_assert.h"
#include "core/json_settings.h"

#include <QDebug>
#include <QSettings>

#include <algorithm>
#include <utility>

namespace akashi {

PermissionRegistry::PermissionRegistry() :
    m_owner_thread(QThread::currentThread())
{}

QString PermissionRegistry::serviceId() const
{
    return QStringLiteral("akashi.permissions");
}

ServiceVersion PermissionRegistry::serviceVersion() const
{
    return {2, 0, 0};
}

bool PermissionRegistry::registerPermission(const PermissionInfo &f_info, const QString &f_owner_id)
{
    AKASHI_ASSERT_OWNER_THREAD();
    if (m_permissions.contains(f_info.id)) {
        return false;
    }
    m_permissions.insert(f_info.id, {f_info, f_owner_id});
    return true;
}

void PermissionRegistry::unregisterAllPermissions(const QString &f_owner_id)
{
    AKASHI_ASSERT_OWNER_THREAD();
    m_permissions.removeIf([&f_owner_id](std::pair<const QString &, PermissionEntry &> f_item) {
        return f_item.second.owner_id == f_owner_id;
    });
}

bool PermissionRegistry::isRegistered(const QString &f_permission_id) const
{
    AKASHI_ASSERT_OWNER_THREAD();
    return m_permissions.contains(f_permission_id);
}

QList<PermissionInfo> PermissionRegistry::permissions() const
{
    AKASHI_ASSERT_OWNER_THREAD();
    QList<PermissionInfo> l_result;
    l_result.reserve(m_permissions.size());
    for (const auto &l_entry : m_permissions) {
        l_result.append(l_entry.info);
    }
    return l_result;
}

QList<PermissionInfo> PermissionRegistry::permissionsByCategory(const QString &f_category) const
{
    AKASHI_ASSERT_OWNER_THREAD();
    QList<PermissionInfo> l_result;
    for (const auto &l_entry : m_permissions) {
        if (l_entry.info.category == f_category) {
            l_result.append(l_entry.info);
        }
    }
    return l_result;
}

std::optional<PermissionInfo> PermissionRegistry::permissionById(const QString &f_permission_id) const
{
    AKASHI_ASSERT_OWNER_THREAD();
    if (auto it = m_permissions.constFind(f_permission_id); it != m_permissions.constEnd()) {
        return it->info;
    }
    return std::nullopt;
}

bool PermissionRegistry::addGrant(const Grant &f_grant)
{
    AKASHI_ASSERT_OWNER_THREAD();
    if (!m_permissions.contains(f_grant.permission)) {
        qCWarning(akashiServer) << "grant refused: unknown permission" << f_grant.permission << "(owner" << f_grant.owner << ")";
        return false;
    }
    // Granting the same offer twice is a no-op, so repeatable commands
    // like /permitsaving never stack duplicate entries.
    if (m_server_grants.contains(f_grant)) {
        return true;
    }
    m_server_grants.append(f_grant);
    return true;
}

bool PermissionRegistry::removeGrant(const Grant &f_grant)
{
    AKASHI_ASSERT_OWNER_THREAD();
    return m_server_grants.removeOne(f_grant);
}

void PermissionRegistry::removeGrantsByOwner(const QString &f_owner_id)
{
    AKASHI_ASSERT_OWNER_THREAD();
    m_server_grants.removeIf([&f_owner_id](const Grant &g) {
        return g.owner == f_owner_id;
    });
}

QList<Grant> PermissionRegistry::serverGrants() const
{
    AKASHI_ASSERT_OWNER_THREAD();
    return m_server_grants;
}

bool PermissionRegistry::registerGrantSource(const QString &f_source_id, GrantSource f_source, const QString &f_owner_id)
{
    AKASHI_ASSERT_OWNER_THREAD();
    for (const auto &l_entry : m_sources) {
        if (l_entry.source_id == f_source_id) {
            qCWarning(akashiServer) << "grant source" << f_source_id << "already registered by" << l_entry.owner_id << "- refused for" << f_owner_id;
            return false;
        }
    }
    m_sources.append({f_source_id, std::move(f_source), f_owner_id});
    return true;
}

void PermissionRegistry::unregisterAllGrantSources(const QString &f_owner_id)
{
    AKASHI_ASSERT_OWNER_THREAD();
    m_sources.removeIf([&f_owner_id](const SourceEntry &e) {
        return e.owner_id == f_owner_id;
    });
}

bool PermissionRegistry::registerSanctionMask(const QString &f_sanction_id, const QString &f_permission_id, const QString &f_owner_id)
{
    AKASHI_ASSERT_OWNER_THREAD();
    if (!m_permissions.contains(f_permission_id)) {
        qCWarning(akashiServer) << "sanction mask refused: unknown permission" << f_permission_id;
        return false;
    }
    if (auto it = m_sanction_masks.constFind(f_sanction_id); it != m_sanction_masks.constEnd()) {
        qCWarning(akashiServer) << "sanction mask for" << f_sanction_id << "already registered by" << it->owner_id << "- refused for" << f_owner_id;
        return false;
    }
    m_sanction_masks.insert(f_sanction_id, {f_permission_id, f_owner_id});
    return true;
}

void PermissionRegistry::unregisterAllSanctionMasks(const QString &f_owner_id)
{
    AKASHI_ASSERT_OWNER_THREAD();
    m_sanction_masks.removeIf([&f_owner_id](std::pair<const QString &, MaskEntry &> f_item) {
        return f_item.second.owner_id == f_owner_id;
    });
}

bool PermissionRegistry::audienceCovers(const Audience &f_audience, const PermissionQuery &f_query)
{
    switch (f_audience.kind) {
    case AudienceKind::Everyone:
        return f_query.is_joined;
    case AudienceKind::Role:
        return f_query.is_authenticated && f_query.acl_role_id.compare(f_audience.role_id, Qt::CaseInsensitive) == 0;
    case AudienceKind::Person:
        return !f_query.ipid.isEmpty() && f_query.ipid == f_audience.person_key;
    case AudienceKind::Participants:
        // Defined with the spectator work; until then nothing is offered
        // through it.
        return false;
    }
    return false;
}

bool PermissionRegistry::isMasked(const PermissionQuery &f_query) const
{
    for (const QString &l_sanction : f_query.sanctions) {
        if (auto it = m_sanction_masks.constFind(l_sanction); it != m_sanction_masks.constEnd() && it->permission_id == f_query.permission) {
            return true;
        }
    }
    return false;
}

bool PermissionRegistry::resolve(const PermissionQuery &f_query) const
{
    AKASHI_ASSERT_OWNER_THREAD();
    // No requirement is the one trivially-true query.
    if (f_query.permission.isEmpty() || f_query.permission == permission::none) {
        return true;
    }
    // The mask beats any grant, so a masked permission never walks the union.
    if (isMasked(f_query)) {
        return false;
    }
    for (const Grant &l_grant : m_server_grants) {
        if (l_grant.permission == f_query.permission && audienceCovers(l_grant.audience, f_query)) {
            return true;
        }
    }
    for (const auto &l_source : m_sources) {
        if (!l_source.source(f_query).isEmpty()) {
            return true;
        }
    }
    return false;
}

Resolution PermissionRegistry::resolveExplained(const PermissionQuery &f_query) const
{
    AKASHI_ASSERT_OWNER_THREAD();
    Resolution l_resolution;
    if (f_query.permission.isEmpty() || f_query.permission == permission::none) {
        l_resolution.allowed = true;
        return l_resolution;
    }
    for (const QString &l_sanction : f_query.sanctions) {
        if (auto it = m_sanction_masks.constFind(l_sanction); it != m_sanction_masks.constEnd() && it->permission_id == f_query.permission) {
            l_resolution.masked_by.append(l_sanction);
        }
    }
    l_resolution.masked_by.sort();
    for (const Grant &l_grant : m_server_grants) {
        if (l_grant.permission == f_query.permission && audienceCovers(l_grant.audience, f_query)) {
            l_resolution.matched.append({l_grant, QStringLiteral("server_grants")});
        }
    }
    for (const auto &l_source : m_sources) {
        const QList<Grant> l_offers = l_source.source(f_query);
        for (const Grant &l_grant : l_offers) {
            l_resolution.matched.append({l_grant, l_source.source_id});
        }
    }
    // Fixed presentation order: scope tier first (server, floor, area),
    // person-audience entries last, then owner and permission - never
    // container iteration order, which Qt randomizes per process.
    std::stable_sort(l_resolution.matched.begin(), l_resolution.matched.end(),
                     [](const GrantMatch &a, const GrantMatch &b) {
                         const auto tier = [](const GrantMatch &m) {
                             if (m.grant.audience.kind == AudienceKind::Person) {
                                 return 3;
                             }
                             return static_cast<int>(m.grant.scope);
                         };
                         if (tier(a) != tier(b)) {
                             return tier(a) < tier(b);
                         }
                         if (a.grant.owner != b.grant.owner) {
                             return a.grant.owner < b.grant.owner;
                         }
                         return a.grant.permission < b.grant.permission;
                     });
    l_resolution.allowed = l_resolution.masked_by.isEmpty() && !l_resolution.matched.isEmpty();
    return l_resolution;
}

const QString ACLRolesHandler::NONE_ID = "NONE";

const QString ACLRolesHandler::SUPER_ID = "SUPER";

// QStringLiteral instead of permission::super: the initialization order of
// this static against the header's inline permission ids is not guaranteed.
const QHash<QString, ACLRole> ACLRolesHandler::readonly_roles{
    {ACLRolesHandler::NONE_ID, ACLRole()},
    {ACLRolesHandler::SUPER_ID, ACLRole({QStringLiteral("super")})},
};

ACLRole::ACLRole() {}

ACLRole::ACLRole(const QSet<QString> &f_permissions) :
    m_permissions(f_permissions)
{}

bool ACLRole::canPerform(const QString &f_permission) const
{
    if (f_permission.isEmpty() || f_permission == permission::none) {
        return true;
    }
    if (m_permissions.contains(permission::super)) {
        return true;
    }
    return m_permissions.contains(f_permission);
}

void ACLRole::setPermission(const QString &f_permission, bool f_mode)
{
    if (f_mode) {
        m_permissions.insert(f_permission);
    }
    else {
        m_permissions.remove(f_permission);
    }
}

const QSet<QString> &ACLRole::permissions() const
{
    return m_permissions;
}

ACLRolesHandler::ACLRolesHandler(QObject *parent) :
    QObject(parent)
{}

ACLRolesHandler::~ACLRolesHandler() {}

bool ACLRolesHandler::roleExists(const QString &f_id) const
{
    AKASHI_ASSERT_THREAD_AFFINITY();
    const QString l_id = f_id.toUpper();
    return readonly_roles.contains(l_id) || m_roles.contains(l_id);
}

ACLRole ACLRolesHandler::roleById(const QString &f_id) const
{
    AKASHI_ASSERT_THREAD_AFFINITY();
    const QString l_id = f_id.toUpper();
    return readonly_roles.contains(l_id) ? readonly_roles.value(l_id) : m_roles.value(l_id);
}

bool ACLRolesHandler::insertRole(QString f_id, ACLRole f_role)
{
    AKASHI_ASSERT_THREAD_AFFINITY();
    f_id = f_id.toUpper();
    if (readonly_roles.contains(f_id)) {
        return false;
    }
    m_roles.insert(f_id, f_role);
    return true;
}

bool ACLRolesHandler::removeRole(QString f_id)
{
    AKASHI_ASSERT_THREAD_AFFINITY();
    f_id = f_id.toUpper();
    if (readonly_roles.contains(f_id)) {
        return false;
    }
    else if (!m_roles.contains(f_id)) {
        return false;
    }
    m_roles.remove(f_id);
    return true;
}

void ACLRolesHandler::clearRoles()
{
    AKASHI_ASSERT_THREAD_AFFINITY();
    m_roles.clear();
}

QString ACLRolesHandler::groupKey(const QString &f_reference)
{
    QString l_key = f_reference.toLower();
    if (l_key.startsWith(QLatin1Char('@'))) {
        l_key.remove(0, 1);
    }
    return l_key;
}

bool ACLRolesHandler::groupExists(const QString &f_reference) const
{
    AKASHI_ASSERT_THREAD_AFFINITY();
    return m_groups.contains(groupKey(f_reference));
}

QStringList ACLRolesHandler::groupMembers(const QString &f_reference) const
{
    AKASHI_ASSERT_THREAD_AFFINITY();
    return m_groups.value(groupKey(f_reference));
}

QStringList ACLRolesHandler::roleIds() const
{
    AKASHI_ASSERT_THREAD_AFFINITY();
    QStringList l_ids = m_roles.keys();
    l_ids += readonly_roles.keys();
    l_ids.sort();
    return l_ids;
}

QStringList ACLRolesHandler::groupNames() const
{
    AKASHI_ASSERT_THREAD_AFFINITY();
    QStringList l_names = m_groups.keys();
    l_names.sort();
    return l_names;
}

std::optional<QStringList> ACLRolesHandler::everyoneBaseline() const
{
    AKASHI_ASSERT_THREAD_AFFINITY();
    return m_everyone;
}

// One permission name as authored, mapped through the shared alias table
// with the one-release warning; a current name passes through unchanged.
static QStringList resolveLegacyName(const QString &f_name, const QString &f_where)
{
    const auto l_alias = permission::legacyAliases().constFind(f_name);
    if (l_alias == permission::legacyAliases().constEnd()) {
        return {f_name};
    }
    qCWarning(akashiConfig) << "permissions:" << f_where << "grants" << f_name << "by its legacy name - now" << l_alias->join(QStringLiteral(", ")) << "- update the file; this alias goes away in a later release.";
    return *l_alias;
}

// Expands one group to leaf permissions, following nested @references.
// The trail refuses cycles; a group in a cycle expands to what it held
// before the loop closed.
static QStringList expandGroup(const QString &f_name, const QHash<QString, QStringList> &f_raw,
                               QStringList &f_trail)
{
    if (f_trail.contains(f_name)) {
        qCWarning(akashiConfig) << "permissions: group @" + f_name << "references itself through" << f_trail.join(QStringLiteral(" > ")) << "- the cycle was cut";
        return {};
    }
    f_trail.append(f_name);
    QStringList l_members;
    const QStringList l_raw_members = f_raw.value(f_name);
    for (const QString &l_member : l_raw_members) {
        if (l_member.startsWith(QLatin1Char('@'))) {
            const QString l_nested = l_member.mid(1).toLower();
            if (!f_raw.contains(l_nested)) {
                qCWarning(akashiConfig) << "permissions: group @" + f_name << "references unknown group" << l_member;
                continue;
            }
            l_members += expandGroup(l_nested, f_raw, f_trail);
        }
        else if (!l_member.isEmpty()) {
            l_members += resolveLegacyName(l_member.toLower(), QStringLiteral("group @") + f_name);
        }
    }
    f_trail.removeAll(f_name);
    l_members.removeDuplicates();
    return l_members;
}

bool ACLRolesHandler::loadFile(QString f_file_name)
{
    AKASHI_ASSERT_THREAD_AFFINITY();
    // JSON files use the custom format, anything else stays INI.
    QSettings l_settings(f_file_name, f_file_name.endsWith(".json") ? JsonSettings::format() : QSettings::IniFormat);
    if (!checkSettingsStatus(&l_settings)) {
        return false;
    }

    // The groups section first, so role sections can reference them. A
    // group's value is its member list: permissions and nested @groups.
    m_groups.clear();
    QHash<QString, QStringList> l_raw_groups;
    const QStringList l_sections = l_settings.childGroups();
    for (const QString &l_section : l_sections) {
        if (l_section.compare(QStringLiteral("groups"), Qt::CaseInsensitive) != 0) {
            continue;
        }
        l_settings.beginGroup(l_section);
        const QStringList l_group_keys = l_settings.childKeys();
        for (const QString &i_key : l_group_keys) {
            l_raw_groups.insert(groupKey(i_key), l_settings.value(i_key).toStringList());
        }
        l_settings.endGroup();
    }
    for (auto it = l_raw_groups.constBegin(); it != l_raw_groups.constEnd(); ++it) {
        QStringList l_trail;
        m_groups.insert(it.key(), expandGroup(it.key(), l_raw_groups, l_trail));
    }

    // The reserved everyone section: not a role anyone wears, but the
    // baseline of ordinary operations offered to every joined person. It
    // reads exactly like a role body - @groups expand, legacy names
    // translate - and lands as a plain list for the caller to grant.
    m_everyone.reset();
    for (const QString &l_section : l_sections) {
        if (l_section.compare(QStringLiteral("everyone"), Qt::CaseInsensitive) != 0) {
            continue;
        }
        QStringList l_members;
        l_settings.beginGroup(l_section);
        const QStringList l_keys = l_settings.childKeys();
        for (const QString &i_key : l_keys) {
            if (!l_settings.value(i_key).toBool()) {
                continue;
            }
            if (i_key.startsWith(QLatin1Char('@'))) {
                if (!m_groups.contains(groupKey(i_key))) {
                    qCWarning(akashiConfig) << "permissions: the everyone section references unknown group" << i_key;
                    continue;
                }
                l_members += m_groups.value(groupKey(i_key));
            }
            else {
                l_members += resolveLegacyName(i_key.toLower(), QStringLiteral("the everyone section"));
            }
        }
        l_settings.endGroup();
        l_members.removeDuplicates();
        m_everyone = l_members;
    }

    m_roles.clear();
    QStringList l_role_records;
    const QStringList l_group_list = l_settings.childGroups();
    for (const QString &i_group : l_group_list) {
        if (i_group.compare(QStringLiteral("groups"), Qt::CaseInsensitive) == 0 ||
            i_group.compare(QStringLiteral("everyone"), Qt::CaseInsensitive) == 0) {
            continue;
        }
        const QString l_upper_group = i_group.toUpper();
        if (readonly_roles.contains(l_upper_group)) {
            qCWarning(akashiConfig) << "permissions: cannot modify role;" << i_group << "is read-only";
            continue;
        }

        l_settings.beginGroup(i_group);
        if (l_role_records.contains(l_upper_group)) {
            qCWarning(akashiConfig) << "permissions: role" << l_upper_group << "already exists";
            continue;
        }
        l_role_records.append(l_upper_group);

        ACLRole l_role;
        const QStringList l_keys = l_settings.childKeys();
        for (const QString &i_key : l_keys) {
            if (!l_settings.value(i_key).toBool()) {
                continue;
            }
            if (i_key.startsWith(QLatin1Char('@'))) {
                // A keyring: the reference expands to the group's leaves.
                // An unknown group is the loud failure the sigil exists
                // for - never a silently accepted mystery permission.
                if (!m_groups.contains(groupKey(i_key))) {
                    qCWarning(akashiConfig) << "permissions: role" << l_upper_group << "references unknown group" << i_key;
                    continue;
                }
                const QStringList l_members = m_groups.value(groupKey(i_key));
                for (const QString &l_member : l_members) {
                    l_role.setPermission(l_member, true);
                }
            }
            else {
                const QStringList l_names = resolveLegacyName(i_key.toLower(), QStringLiteral("role ") + l_upper_group);
                for (const QString &l_name : l_names) {
                    l_role.setPermission(l_name, true);
                }
            }
        }
        m_roles.insert(l_upper_group, std::move(l_role));
        l_settings.endGroup();
    }

    return true;
}

bool ACLRolesHandler::saveFile(QString f_file_name)
{
    AKASHI_ASSERT_THREAD_AFFINITY();
    // JSON files use the custom format, anything else stays INI.
    QSettings l_settings(f_file_name, f_file_name.endsWith(".json") ? JsonSettings::format() : QSettings::IniFormat);
    if (!checkSettingsStatus(&l_settings)) {
        return false;
    }

    l_settings.clear();
    const QStringList l_role_id_list = m_roles.keys();
    for (const QString &l_role_id : l_role_id_list) {
        const QString l_upper_role_id = l_role_id.toUpper();
        if (readonly_roles.contains(l_upper_role_id)) {
            continue;
        }

        const ACLRole i_role = m_roles.value(l_upper_role_id);
        l_settings.beginGroup(l_upper_role_id);
        if (i_role.canPerform(permission::super)) {
            l_settings.setValue(permission::super, true);
        }
        else {
            QStringList l_permissions(i_role.permissions().begin(), i_role.permissions().end());
            l_permissions.sort();
            for (const QString &i_permission : std::as_const(l_permissions)) {
                l_settings.setValue(i_permission, true);
            }
        }
        l_settings.endGroup();
    }

    // The everyone section rides along, already expanded to its leaves -
    // a user-management save must never silently delete the baseline.
    if (m_everyone.has_value()) {
        l_settings.beginGroup(QStringLiteral("everyone"));
        QStringList l_members = m_everyone.value();
        l_members.sort();
        for (const QString &i_member : std::as_const(l_members)) {
            l_settings.setValue(i_member, true);
        }
        l_settings.endGroup();
    }

    l_settings.sync();
    if (l_settings.status() != QSettings::NoError) {
        qCWarning(akashiConfig) << "acl_roles: failed to write file; aborting (" << f_file_name << ")";
        return false;
    }

    return true;
}

bool ACLRolesHandler::checkSettingsStatus(QSettings *f_settings)
{
    if (f_settings->status() != QSettings::NoError) {
        switch (f_settings->status()) {
        case QSettings::AccessError:
            qCWarning(akashiConfig) << "acl_roles: failed to open file; aborting (" << f_settings->fileName() << ")";
            break;

        case QSettings::FormatError:
            qCWarning(akashiConfig) << "acl_roles: file is malformed; aborting (" << f_settings->fileName() << ")";
            break;

        default:
            qCWarning(akashiConfig) << "acl_roles: unknown error; aborting (" << f_settings->fileName() << ")";
            break;
        }

        return false;
    }
    return true;
}

} // namespace akashi
