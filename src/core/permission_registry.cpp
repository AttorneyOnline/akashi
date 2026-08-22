#include "core/permission_registry.h"

#include "akashi/logging_categories.h"
#include "akashi/thread_assert.h"
#include "core/json_settings.h"

#include <QDebug>
#include <QSettings>

#include <algorithm>
#include <utility>

namespace akashi {

QString describeGrant(const Grant &f_grant, const QString &f_place_name)
{
    QString l_audience;
    switch (f_grant.audience.kind) {
    case AudienceKind::Everyone:
        l_audience = QStringLiteral("everyone");
        break;
    case AudienceKind::Participants:
        l_audience = QStringLiteral("participants");
        break;
    case AudienceKind::Role:
        l_audience = QStringLiteral("role ") + f_grant.audience.role_id;
        break;
    case AudienceKind::Person:
        l_audience = QStringLiteral("person ") + f_grant.audience.person_key;
        break;
    }
    const QString l_place = f_place_name.isEmpty() ? QString::number(f_grant.place_id) : f_place_name;
    QString l_where;
    switch (f_grant.scope) {
    case GrantScope::Server:
        l_where = QStringLiteral("server");
        break;
    case GrantScope::Floor:
        l_where = QStringLiteral("floor ") + l_place;
        break;
    case GrantScope::Area:
        l_where = QStringLiteral("area ") + l_place;
        break;
    }
    return f_grant.permission + QStringLiteral(" -> ") + l_audience + QStringLiteral(" @ ") + l_where + QStringLiteral(" [") + f_grant.owner + QStringLiteral("]");
}

PermissionRegistry::PermissionRegistry() :
    m_owner_thread(QThread::currentThread())
{}

QString PermissionRegistry::serviceId() const
{
    return QStringLiteral("akashi.permissions");
}

ServiceVersion PermissionRegistry::serviceVersion() const
{
    return {3, 0, 0};
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

QStringList PermissionRegistry::unregisterAllPermissions(const QString &f_owner_id)
{
    AKASHI_ASSERT_OWNER_THREAD();
    const QStringList l_dropped = permissionsOwnedBy(f_owner_id);
    m_permissions.removeIf([&f_owner_id](std::pair<const QString &, PermissionEntry &> f_item) {
        return f_item.second.owner_id == f_owner_id;
    });
    // An offer naming a permission that no longer exists would keep
    // resolving by name alone, so the catalog entry and every offer that
    // leans on it leave together.
    removeGrantsNaming(QSet<QString>(l_dropped.begin(), l_dropped.end()));
    return l_dropped;
}

void PermissionRegistry::removeGrantsNaming(const QSet<QString> &f_permissions)
{
    for (const QString &l_permission : f_permissions) {
        m_grants.remove(l_permission);
    }
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

QStringList PermissionRegistry::permissionsOwnedBy(const QString &f_owner_id) const
{
    AKASHI_ASSERT_OWNER_THREAD();
    QStringList l_ids;
    for (auto it = m_permissions.constBegin(); it != m_permissions.constEnd(); ++it) {
        if (it.value().owner_id == f_owner_id) {
            l_ids << it.key();
        }
    }
    l_ids.sort();
    return l_ids;
}

std::optional<Grant> PermissionRegistry::admit(const Grant &f_grant, const QString &f_where) const
{
    AKASHI_ASSERT_OWNER_THREAD();
    const auto l_entry = m_permissions.constFind(f_grant.permission);
    if (l_entry == m_permissions.constEnd()) {
        qCWarning(akashiServer) << f_where << "grants unknown permission" << f_grant.permission << "- the grant was skipped.";
        return std::nullopt;
    }
    // The guard rail: the hammers never ride an everyone-shaped offer - a
    // room can be handed the jukebox, never the mute. Asked here rather
    // than at the config compile, so the plugin and script doors meet it
    // too, and asked on the expanded name, so a group cannot smuggle a
    // member past it.
    const bool l_everyone_shaped = f_grant.audience.kind == AudienceKind::Everyone || f_grant.audience.kind == AudienceKind::Participants;
    if (l_everyone_shaped && l_entry->info.restricted) {
        qCCritical(akashiServer) << f_where << "tries to offer" << f_grant.permission
                                 << "to everyone - moderation and administration powers are role grants. The grant was refused.";
        return std::nullopt;
    }
    return f_grant;
}

bool PermissionRegistry::addGrant(const Grant &f_grant)
{
    AKASHI_ASSERT_OWNER_THREAD();
    const std::optional<Grant> l_admitted = admit(f_grant, QStringLiteral("the grant from ") + f_grant.owner);
    if (!l_admitted) {
        return false;
    }
    QList<Grant> &l_bucket = m_grants[l_admitted->permission];
    // Granting the same offer twice is a no-op, so repeatable commands
    // like /permitsaving never stack duplicate entries.
    if (!l_bucket.contains(*l_admitted)) {
        l_bucket.append(*l_admitted);
    }
    return true;
}

bool PermissionRegistry::removeGrant(const Grant &f_grant)
{
    AKASHI_ASSERT_OWNER_THREAD();
    const auto l_bucket = m_grants.find(f_grant.permission);
    if (l_bucket == m_grants.end()) {
        return false;
    }
    const bool l_removed = l_bucket->removeOne(f_grant);
    if (l_bucket->isEmpty()) {
        m_grants.erase(l_bucket);
    }
    return l_removed;
}

void PermissionRegistry::removeGrantsByOwner(const QString &f_owner_id)
{
    AKASHI_ASSERT_OWNER_THREAD();
    for (auto it = m_grants.begin(); it != m_grants.end();) {
        it->removeIf([&f_owner_id](const Grant &g) {
            return g.owner == f_owner_id;
        });
        it = it->isEmpty() ? m_grants.erase(it) : std::next(it);
    }
}

QList<Grant> PermissionRegistry::grants() const
{
    AKASHI_ASSERT_OWNER_THREAD();
    QList<Grant> l_all;
    for (const QList<Grant> &l_bucket : m_grants) {
        l_all += l_bucket;
    }
    // A hash iterates in whatever order it likes, and the --check-config
    // dump is diffed between runs, so the order is decided here.
    std::sort(l_all.begin(), l_all.end(), [](const Grant &a, const Grant &b) {
        if (a.permission != b.permission) {
            return a.permission < b.permission;
        }
        if (a.scope != b.scope) {
            return a.scope < b.scope;
        }
        if (a.place_id != b.place_id) {
            return a.place_id < b.place_id;
        }
        if (a.owner != b.owner) {
            return a.owner < b.owner;
        }
        return static_cast<int>(a.audience.kind) < static_cast<int>(b.audience.kind);
    });
    return l_all;
}

void PermissionRegistry::setRoleProvider(RoleProvider f_provider)
{
    AKASHI_ASSERT_OWNER_THREAD();
    m_role_provider = std::move(f_provider);
}

void PermissionRegistry::setRoleHolds(RoleHoldsFn f_role_holds)
{
    AKASHI_ASSERT_OWNER_THREAD();
    m_role_holds = std::move(f_role_holds);
}

QStringList PermissionRegistry::rolesWorn(const PermissionQuery &f_query) const
{
    if (m_role_provider) {
        return m_role_provider(f_query);
    }
    // Without a host provider the only role anyone wears is the one they
    // authenticated into.
    if (f_query.is_authenticated && !f_query.acl_role_id.isEmpty()) {
        return {f_query.acl_role_id};
    }
    return {};
}

bool PermissionRegistry::registerGrantSource(const QString &f_source_id, const QString &f_because, GrantSource f_source,
                                             const QString &f_owner_id, const QSet<QString> &f_interests)
{
    AKASHI_ASSERT_OWNER_THREAD();
    for (const auto &l_entry : m_sources) {
        if (l_entry.source_id == f_source_id) {
            qCWarning(akashiServer) << "grant source" << f_source_id << "already registered by" << l_entry.owner_id << "- refused for" << f_owner_id;
            return false;
        }
    }
    m_sources.append({f_source_id, f_because, std::move(f_source), f_owner_id, f_interests});
    return true;
}

bool PermissionRegistry::setSourceInterests(const QString &f_source_id, const QSet<QString> &f_interests)
{
    AKASHI_ASSERT_OWNER_THREAD();
    for (SourceEntry &l_entry : m_sources) {
        if (l_entry.source_id == f_source_id) {
            l_entry.interests = f_interests;
            return true;
        }
    }
    return false;
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
    // One sanction may take away several acts; the same act twice is the
    // refusal, since two owners would each think they held that mask.
    QList<MaskEntry> &l_masks = m_sanction_masks[f_sanction_id];
    for (const MaskEntry &l_mask : l_masks) {
        if (l_mask.permission_id == f_permission_id) {
            qCWarning(akashiServer) << "sanction" << f_sanction_id << "already masks" << f_permission_id
                                    << "- registered by" << l_mask.owner_id << ", refused for" << f_owner_id;
            return false;
        }
    }
    l_masks.append({f_permission_id, f_owner_id});
    return true;
}

void PermissionRegistry::unregisterAllSanctionMasks(const QString &f_owner_id)
{
    AKASHI_ASSERT_OWNER_THREAD();
    for (auto it = m_sanction_masks.begin(); it != m_sanction_masks.end();) {
        it->removeIf([&f_owner_id](const MaskEntry &f_mask) {
            return f_mask.owner_id == f_owner_id;
        });
        it = it->isEmpty() ? m_sanction_masks.erase(it) : std::next(it);
    }
}

bool PermissionRegistry::audienceCovers(const Audience &f_audience, const PermissionQuery &f_query,
                                        const QStringList &f_worn_roles)
{
    switch (f_audience.kind) {
    case AudienceKind::Everyone:
        return f_query.is_joined;
    case AudienceKind::Role:
        return std::any_of(f_worn_roles.cbegin(), f_worn_roles.cend(), [&f_audience](const QString &f_role) {
            return f_role.compare(f_audience.role_id, Qt::CaseInsensitive) == 0;
        });
    case AudienceKind::Person:
        return !f_query.ipid.isEmpty() && f_query.ipid == f_audience.person_key;
    case AudienceKind::Participants:
        // Defined with the spectator work; until then nothing is offered
        // through it.
        return false;
    }
    return false;
}

bool PermissionRegistry::scopeCovers(const Grant &f_grant, const PermissionQuery &f_query)
{
    switch (f_grant.scope) {
    case GrantScope::Server:
        return true;
    case GrantScope::Floor:
        return f_grant.place_id == f_query.floor_id;
    case GrantScope::Area:
        return f_grant.place_id == f_query.area_id;
    }
    return false;
}

// Where one contribution sits in the printed answer: the roles you wear,
// then standing offers from the widest scope inward with personal ones
// last, then the live facts.
static int contributionTier(const Contribution &f_contribution)
{
    switch (f_contribution.kind) {
    case Contribution::Kind::Role:
        return 0;
    case Contribution::Kind::Grant:
        if (f_contribution.grant->audience.kind == AudienceKind::Person) {
            return 4;
        }
        return 1 + static_cast<int>(f_contribution.grant->scope);
    case Contribution::Kind::Source:
        return 5;
    }
    return 5;
}

bool PermissionRegistry::resolve(const PermissionQuery &f_query) const
{
    return resolveInternal(f_query, Mode::StopAtFirst).allowed;
}

Resolution PermissionRegistry::resolveExplained(const PermissionQuery &f_query) const
{
    return resolveInternal(f_query, Mode::CollectAll);
}

Resolution PermissionRegistry::resolveInternal(const PermissionQuery &f_query, Mode f_mode) const
{
    AKASHI_ASSERT_OWNER_THREAD();
    Resolution l_resolution;

    // No requirement is the one trivially-true query.
    if (f_query.permission.isEmpty() || f_query.permission == permission::none) {
        l_resolution.allowed = true;
        return l_resolution;
    }

    // The subtraction, asked first because it outranks everything that
    // follows - a masked act never walks the union.
    for (const QString &l_sanction : f_query.sanctions) {
        const auto l_masks = m_sanction_masks.constFind(l_sanction);
        if (l_masks == m_sanction_masks.constEnd()) {
            continue;
        }
        for (const MaskEntry &l_mask : *l_masks) {
            if (l_mask.permission_id != f_query.permission) {
                continue;
            }
            l_resolution.masked_by.append(l_sanction);
            if (f_mode == Mode::StopAtFirst) {
                return l_resolution;
            }
            break;
        }
    }

    const QStringList l_worn = rolesWorn(f_query);

    // A role worn here holds the act outright. This is the one place
    // super lives - the roles handler answers for it.
    if (m_role_holds) {
        for (const QString &l_role : l_worn) {
            if (!m_role_holds(l_role, f_query.permission)) {
                continue;
            }
            l_resolution.contributions.append({Contribution::Kind::Role, l_role, {}, std::nullopt});
            if (f_mode == Mode::StopAtFirst) {
                l_resolution.allowed = true;
                return l_resolution;
            }
        }
    }

    // The standing offers that name this act, whatever scope they stand
    // in - one bucket, not a walk over every grant the server holds.
    const QList<Grant> l_stored = m_grants.value(f_query.permission);
    for (const Grant &l_grant : l_stored) {
        if (!scopeCovers(l_grant, f_query) || !audienceCovers(l_grant.audience, f_query, l_worn)) {
            continue;
        }
        l_resolution.contributions.append({Contribution::Kind::Grant, {}, {}, l_grant});
        if (f_mode == Mode::StopAtFirst) {
            l_resolution.allowed = true;
            return l_resolution;
        }
    }

    // The live facts, asked only where they said they could answer.
    for (const SourceEntry &l_source : m_sources) {
        if (!l_source.answers(f_query.permission) || !l_source.source(f_query)) {
            continue;
        }
        l_resolution.contributions.append({Contribution::Kind::Source, l_source.source_id, l_source.because, std::nullopt});
        if (f_mode == Mode::StopAtFirst) {
            l_resolution.allowed = true;
            return l_resolution;
        }
    }

    // Fixed presentation order - never container iteration order, which
    // Qt randomizes per process.
    l_resolution.masked_by.sort();
    std::stable_sort(l_resolution.contributions.begin(), l_resolution.contributions.end(),
                     [](const Contribution &a, const Contribution &b) {
                         if (contributionTier(a) != contributionTier(b)) {
                             return contributionTier(a) < contributionTier(b);
                         }
                         if (a.id != b.id) {
                             return a.id < b.id;
                         }
                         if (a.grant && b.grant && a.grant->owner != b.grant->owner) {
                             return a.grant->owner < b.grant->owner;
                         }
                         return false;
                     });
    l_resolution.allowed = l_resolution.masked_by.isEmpty() && !l_resolution.contributions.isEmpty();
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
