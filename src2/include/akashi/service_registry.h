#ifndef AKASHI_SERVICE_REGISTRY_H
#define AKASHI_SERVICE_REGISTRY_H

#include "akashi/service.h"
#include "akashi_core_export.h"

#include <QHash>
#include <QObject>
#include <QString>

#include <memory>

namespace akashi {

// Holds the services the core and plugins provide, so anyone can find them by id.
// Ownership is tracked so a plugin's services are removed together when it unloads.
class AKASHI_CORE_EXPORT ServiceRegistry : public QObject
{
    Q_OBJECT

  public:
    explicit ServiceRegistry(QObject *parent = nullptr) :
        QObject(parent) {}

    // Registers a service. Fails if the id is already taken.
    bool registerService(std::shared_ptr<IService> f_service, const QString &f_owner_id = QString());

    void unregisterService(const QString &f_service_id);

    // Removes every service registered by the given owner, for plugin unloading.
    void unregisterServicesOwnedBy(const QString &f_owner_id);

    // The service for an id, or null if missing or its version does not match the range.
    std::shared_ptr<IService> find(const QString &f_service_id, const QString &f_version_range = QString()) const;

    bool isAvailable(const QString &f_service_id, const QString &f_version_range = QString()) const;

    // The service cast to its concrete type, or null if missing or of the wrong type.
    template <typename T>
    std::shared_ptr<T> resolve(const QString &f_service_id, const QString &f_version_range = QString()) const
    {
        return std::dynamic_pointer_cast<T>(find(f_service_id, f_version_range));
    }

  Q_SIGNALS:
    void serviceRegistered(const QString &f_service_id);
    void serviceAboutToUnregister(const QString &f_service_id);
    void serviceUnregistered(const QString &f_service_id);

  private:
    struct Entry
    {
        std::shared_ptr<IService> service;
        QString owner;
    };

    QHash<QString, Entry> m_services;
};

} // namespace akashi

#endif // AKASHI_SERVICE_REGISTRY_H
