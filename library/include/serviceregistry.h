#pragma once

// Welcome to absolute hell :)

#include "akashi_addon_global.h"
#include "servicewrapper.h"

#include <QLoggingCategory>
#include <QMap>
#include <QObject>

class Service;

Q_DECLARE_EXPORTED_LOGGING_CATEGORY(akashiServiceRegistry, AKASHI_ADDON_EXPORT)

// Services are commonly owned by the ServiceRegistry.
// Why? Because I say so. - Salanto

class AKASHI_ADDON_EXPORT ServiceRegistry : public QObject
{
    Q_OBJECT

  public:
    ServiceRegistry(QObject *parent = nullptr);
    ~ServiceRegistry();

    template <class T>
        requires std::is_base_of_v<Service, T>
    inline void create()
    {
        T *l_ptr = new T(this);
        l_ptr->setServiceRegistry(this);
        insertService<T>(l_ptr);
    }

    template <class T>
        requires std::is_base_of_v<QObject, T>
    inline void createWrapped(const QString &f_identifier,
                              const QString &f_version, const QString &f_author)
    {
        T *l_ptr = new T(this);
        ServiceWrapper<T> *l_wrapper = new ServiceWrapper<T>(l_ptr, f_identifier, f_version, f_author, this);
        insertService<ServiceWrapper<T>>(l_wrapper);
    }

    template <class T>
        requires std::is_base_of_v<Service, T>
    inline std::optional<T *> get(const QString f_identifier)
    {
        if (!m_services.contains(f_identifier)) {
            qCCritical(akashiServiceRegistry) << qUtf8Printable(QString("Unable to get service with identifier %1").arg(f_identifier));
            return std::nullopt;
        }

        Service *l_service = m_services.value(f_identifier);

        const Service::State l_service_state = l_service->getState();
        if (l_service_state != Service::OK) {
            qCCritical(akashiServiceRegistry) << qUtf8Printable(QString("Unable to get service with identifier %1 due to state: %2").arg(f_identifier).arg(l_service_state));
            return std::nullopt;
        }

        return std::make_optional<T *>(static_cast<T *>(l_service));
    }

    inline bool exists(const QString &f_identifier)
    {
        return m_services.contains(f_identifier);
    }

  private:
    template <class T>
        requires std::is_base_of_v<Service, T>
    void insertService(T *f_ptr)
    {
        std::unique_ptr<T> l_ptr_scoped(f_ptr);
        const QString l_identifier = l_ptr_scoped->getServiceProperty("identifier");
        const QString l_version = l_ptr_scoped->getServiceProperty("version");
        const QString l_author = l_ptr_scoped->getServiceProperty("author");

        if (l_identifier.isEmpty()) {
            qCCritical(akashiServiceRegistry) << "Unable to register service: Service identifier is empty.";
            return;
        }

        if (m_services.contains(l_identifier)) {
            qCCritical(akashiServiceRegistry) << "Unable to register service: Service identifier is already taken.";
            return;
        }

        // We are out of the hot path. We can now safely remove ourselves from this equation.
        T *l_ptr = l_ptr_scoped.release();
        m_services.insert(l_identifier, l_ptr);

        qCInfo(akashiServiceRegistry) << qUtf8Printable(QString("Adding Service: %1:%2:%3").arg(l_identifier, l_version, l_author));
    }

    QMap<QString, Service *> m_services;
};
