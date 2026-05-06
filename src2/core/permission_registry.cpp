#include "core/permission_registry.h"

#include <algorithm>

namespace akashi {

bool PermissionRegistry::registerPermission(const PermissionInfo &f_info, const QString &f_owner_id)
{
    if (m_permissions.contains(f_info.id)) {
        return false;
    }
    m_permissions.insert(f_info.id, {f_info, f_owner_id});
    return true;
}

void PermissionRegistry::unregisterAllPermissions(const QString &f_owner_id)
{
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
    return m_permissions.contains(f_permission_id);
}

QList<PermissionInfo> PermissionRegistry::permissions() const
{
    QList<PermissionInfo> l_result;
    l_result.reserve(m_permissions.size());
    for (const auto &l_entry : m_permissions) {
        l_result.append(l_entry.info);
    }
    return l_result;
}

QList<PermissionInfo> PermissionRegistry::permissionsByCategory(const QString &f_category) const
{
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
    if (auto it = m_permissions.constFind(f_permission_id); it != m_permissions.constEnd()) {
        return it->info;
    }
    return std::nullopt;
}

bool PermissionRegistry::registerResolver(const QString &f_resolver_id, int f_priority,
                                          PermissionResolver f_resolver, const QString &f_owner_id)
{
    for (const auto &l_entry : m_resolvers) {
        if (l_entry.resolver_id == f_resolver_id) {
            return false;
        }
    }
    ResolverEntry l_entry{f_resolver_id, f_priority, std::move(f_resolver), f_owner_id};
    auto l_pos = std::lower_bound(m_resolvers.begin(), m_resolvers.end(), l_entry,
                                  [](const ResolverEntry &a, const ResolverEntry &b) {
                                      return a.priority < b.priority;
                                  });
    m_resolvers.insert(l_pos, std::move(l_entry));
    return true;
}

void PermissionRegistry::unregisterAllResolvers(const QString &f_owner_id)
{
    m_resolvers.removeIf([&f_owner_id](const ResolverEntry &e) {
        return e.owner_id == f_owner_id;
    });
}

bool PermissionRegistry::resolve(const PermissionQuery &f_query) const
{
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

} // namespace akashi
