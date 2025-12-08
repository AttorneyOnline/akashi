#include "serviceregistry.h"
#include <service.h>

#include <QDebug>
#include <QMetaClassInfo>

ServiceRegistry::ServiceRegistry(QObject *f_parent) : QObject{f_parent}
{
}

bool ServiceRegistry::hasService(QString identifier)
{
    return m_services.contains(identifier);
}

void ServiceRegistry::registerService(Service *f_pointer)
{
    const QMetaObject *l_metaObject = f_pointer->metaObject();
    const QString l_identifier = getClassInfo(l_metaObject, "Identifier");

    if (hasService(l_identifier) || l_identifier.isEmpty()) {
        qDebug() << "Failed to add service. Identifier already used or empty. Deleting offending object.";
        f_pointer->deleteLater();
        return;
    }

    m_services.insert(l_identifier, f_pointer);
    qDebug() << "Added service with identifier" << l_identifier;
}

void ServiceRegistry::removeService(QString f_identifier)
{
    if (hasService(f_identifier)) {
        Service *l_pointer = m_services.take(f_identifier);
        l_pointer->deleteLater();
        qDebug() << "Deleted service" << f_identifier;
        return;
    }

    qDebug() << "Failed to delete nonexistent service" << f_identifier;
}

std::optional<QString> ServiceRegistry::getServiceInfo(QString f_identifier, QString f_key)
{
    Service *l_service = m_services.value(f_identifier, nullptr);

    if (!l_service) {
        return std::nullopt;
    }

    return std::make_optional<QString>(getClassInfo(l_service->metaObject(), f_key));
}

QString ServiceRegistry::getClassInfo(const QMetaObject *f_metaObject, QString f_key) const
{
    const int l_index = f_metaObject->indexOfClassInfo(f_key.toStdString().c_str());
    QMetaClassInfo l_classInfo = f_metaObject->classInfo(l_index);
    return QString::fromLatin1(l_classInfo.value());
}
