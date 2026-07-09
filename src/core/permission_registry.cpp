#include "core/permission_registry.h"

#include "akashi/logging_categories.h"
#include "akashi/thread_assert.h"
#include "core/json_settings.h"

#include <QDebug>
#include <QSettings>

#include <algorithm>

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
    return {1, 0, 0};
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
    auto it = m_permissions.begin();
    while (it != m_permissions.end()) {
        if (it->owner_id == f_owner_id) {
            it = m_permissions.erase(it);
        }
        else {
            ++it;
        }
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

bool PermissionRegistry::registerResolver(const QString &f_resolver_id, int f_priority,
                                          PermissionResolver f_resolver, const QString &f_owner_id)
{
    AKASHI_ASSERT_OWNER_THREAD();
    for (const auto &l_entry : m_resolvers) {
        if (l_entry.resolver_id == f_resolver_id) {
            return false;
        }
    }
    ResolverEntry l_entry{f_resolver_id, f_priority, std::move(f_resolver), f_owner_id};
    // Insert after any equal priorities, so ties keep registration order
    // and a later resolver can never preempt an earlier one at the same
    // priority - the same tiebreak the text filter registry uses.
    auto l_pos = std::upper_bound(m_resolvers.begin(), m_resolvers.end(), l_entry,
                                  [](const ResolverEntry &a, const ResolverEntry &b) {
                                      return a.priority < b.priority;
                                  });
    m_resolvers.insert(l_pos, std::move(l_entry));
    return true;
}

void PermissionRegistry::unregisterAllResolvers(const QString &f_owner_id)
{
    AKASHI_ASSERT_OWNER_THREAD();
    m_resolvers.removeIf([&f_owner_id](const ResolverEntry &e) {
        return e.owner_id == f_owner_id;
    });
}

bool PermissionRegistry::resolve(const PermissionQuery &f_query) const
{
    AKASHI_ASSERT_OWNER_THREAD();
    for (const auto &l_entry : m_resolvers) {
        PermissionVerdict l_verdict = l_entry.resolver(f_query);
        if (l_verdict == PermissionVerdict::Granted) {
            return true;
        }
        if (l_verdict == PermissionVerdict::Denied) {
            return false;
        }
    }
    return false;
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

bool ACLRolesHandler::roleExists(QString f_id)
{
    f_id = f_id.toUpper();
    return readonly_roles.contains(f_id) || m_roles.contains(f_id);
}

ACLRole ACLRolesHandler::roleById(QString f_id)
{
    f_id = f_id.toUpper();
    return readonly_roles.contains(f_id) ? readonly_roles.value(f_id) : m_roles.value(f_id);
}

bool ACLRolesHandler::insertRole(QString f_id, ACLRole f_role)
{
    f_id = f_id.toUpper();
    if (readonly_roles.contains(f_id)) {
        return false;
    }
    m_roles.insert(f_id, f_role);
    return true;
}

bool ACLRolesHandler::removeRole(QString f_id)
{
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
    m_roles.clear();
}

bool ACLRolesHandler::loadFile(QString f_file_name)
{
    // JSON files use the custom format, anything else stays INI.
    QSettings l_settings(f_file_name, f_file_name.endsWith(".json") ? JsonSettings::format() : QSettings::IniFormat);
    if (!checkSettingsStatus(&l_settings)) {
        return false;
    }

    m_roles.clear();
    QStringList l_role_records;
    const QStringList l_group_list = l_settings.childGroups();
    for (const QString &i_group : l_group_list) {
        const QString l_upper_group = i_group.toUpper();
        if (readonly_roles.contains(l_upper_group)) {
            qCWarning(akashiConfig) << "acl_roles: cannot modify role;" << i_group << "is read-only";
            continue;
        }

        l_settings.beginGroup(i_group);
        if (l_role_records.contains(l_upper_group)) {
            qCWarning(akashiConfig) << "acl_roles: role" << l_upper_group << "already exists";
            continue;
        }
        l_role_records.append(l_upper_group);

        ACLRole l_role;
        const QStringList l_keys = l_settings.childKeys();
        for (const QString &i_key : l_keys) {
            if (l_settings.value(i_key).toBool()) {
                l_role.setPermission(i_key.toLower(), true);
            }
        }
        m_roles.insert(l_upper_group, std::move(l_role));
        l_settings.endGroup();
    }

    return true;
}

bool ACLRolesHandler::saveFile(QString f_file_name)
{
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
