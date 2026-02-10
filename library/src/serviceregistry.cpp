#include "serviceregistry.h"
#include "service.h"
#include "servicewrapper.h"

#include <QDebug>

ServiceRegistry::ServiceRegistry(QObject *parent) : QObject{parent}
{
    qInfo() << "Creating new service registry instance at" << this;
}

void ServiceRegistry::insertService(Service *f_ptr)
{
    std::unique_ptr<Service> l_ptr_scoped(f_ptr);
    const QString l_identifier = l_ptr_scoped->getServiceProperty("identifier");
    const QString l_version = l_ptr_scoped->getServiceProperty("version");
    const QString l_author = l_ptr_scoped->getServiceProperty("author");

    if (l_identifier.isEmpty()) {
        qCritical() << "Unable to register service: Service identifier is empty.";
        return;
    }

    if (m_services.contains(l_identifier)) {
        qCritical() << "Unable to register service: Service identifier is already taken.";
        return;
    }

    // We are out of the hot path. We can now safely remove ourselves from this equation.
    Service *l_ptr = l_ptr_scoped.release();
    m_services.insert(l_identifier, l_ptr);

    qInfo() << QString("Adding Service: %1:%2:%3").arg(l_identifier, l_version, l_author);
}
