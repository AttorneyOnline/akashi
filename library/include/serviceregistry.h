#pragma once

// Welcome to absolute hell :)

#include "akashi_addon_global.h"
#include "servicewrapper.h"

#include <QMap>
#include <QObject>

class Service;

// Services are commonly owned by the ServiceRegistry.
// Why? Because I say so. - Salanto

class AKASHI_ADDON_EXPORT ServiceRegistry : public QObject
{
    Q_OBJECT

  public:
    ServiceRegistry(QObject *parent = nullptr);

    template <class T>
        requires std::is_base_of_v<Service, T>
    inline void create()
    {
        T *l_ptr = new T(this);
        l_ptr->setServiceRegistry(this);
        insertService(l_ptr);
    }

    template <class T>
        requires std::is_base_of_v<QObject, T>
    inline void createWrapped(const QString &f_identifier,
                              const QString &f_version, const QString &f_author)
    {
        T *l_ptr = new T(this);
        ServiceWrapper<T> *l_wrapper = new ServiceWrapper<T>(l_ptr, f_identifier, f_version, f_author, this);
        insertService(l_wrapper);
    }

    template <class T>
        requires std::is_base_of_v<Service, T>
    inline std::optional<T *> get(const QString f_identifier)
    {
        if (!m_services.contains(f_identifier)) {
            qCritical() << "Unable to get service with identifier" << f_identifier;
            return std::nullopt;
        }

        Service *l_service = m_services.value(f_identifier);

        const Service::State l_service_state = l_service->getState();
        if (l_service_state != Service::OK) {
            qCritical() << "Unable to get service with identifier" << f_identifier << "due to state:" << l_service_state;
            return std::nullopt;
        }

        return std::make_optional<T *>(static_cast<T *>(l_service));
    }

  private:
    void insertService(Service *f_ptr);

    QMap<QString, Service *> m_services;
};
