#include "akashi/service_registry.h"

#include "core/thread_assert.h"

#include <QDebug>

namespace akashi {

bool ServiceRegistry::registerService(std::shared_ptr<IService> f_service, const QString &f_owner_id)
{
    AKASHI_ASSERT_THREAD_AFFINITY();
    if (!f_service) {
        return false;
    }
    const QString l_id = f_service->serviceId();
    if (m_services.contains(l_id)) {
        qWarning() << "Service already registered:" << l_id;
        return false;
    }
    m_services.insert(l_id, Entry{f_service, f_owner_id});
    Q_EMIT serviceRegistered(l_id);
    return true;
}

void ServiceRegistry::unregisterService(const QString &f_service_id)
{
    AKASHI_ASSERT_THREAD_AFFINITY();
    if (!m_services.contains(f_service_id)) {
        return;
    }
    Q_EMIT serviceAboutToUnregister(f_service_id);
    m_services.remove(f_service_id);
    Q_EMIT serviceUnregistered(f_service_id);
}

void ServiceRegistry::unregisterServicesOwnedBy(const QString &f_owner_id)
{
    AKASHI_ASSERT_THREAD_AFFINITY();
    // Collected first so the map is not changed while iterating it.
    QStringList l_owned;
    for (auto l_iterator = m_services.constBegin(); l_iterator != m_services.constEnd(); ++l_iterator) {
        if (l_iterator.value().owner == f_owner_id) {
            l_owned.append(l_iterator.key());
        }
    }
    for (const QString &l_id : l_owned) {
        unregisterService(l_id);
    }
}

std::shared_ptr<IService> ServiceRegistry::find(const QString &f_service_id, const QString &f_version_range) const
{
    AKASHI_ASSERT_THREAD_AFFINITY();
    const auto l_iterator = m_services.constFind(f_service_id);
    if (l_iterator == m_services.constEnd()) {
        return nullptr;
    }
    if (!f_version_range.isEmpty() && !l_iterator.value().service->serviceVersion().satisfies(f_version_range)) {
        return nullptr;
    }
    return l_iterator.value().service;
}

bool ServiceRegistry::isAvailable(const QString &f_service_id, const QString &f_version_range) const
{
    AKASHI_ASSERT_THREAD_AFFINITY();
    return find(f_service_id, f_version_range) != nullptr;
}

} // namespace akashi
