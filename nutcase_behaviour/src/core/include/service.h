#pragma once

#include <QMetaClassInfo>
#include <QObject>

#include "akashi_core_global.h"

class ServiceRegistry;

class AKASHI_CORE_EXPORT Service : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("Author", "Salanto")
    Q_CLASSINFO("Version", "1.0.0")
    Q_CLASSINFO("Identifier", "akashi.service");

  public:
    Service(ServiceRegistry *f_registry, QObject *parent = nullptr) :
        QObject{parent},
        m_registry{f_registry} {};
    ~Service() = default;

    QString getClassValue(QString f_key) const;

  protected:
    ServiceRegistry *m_registry;
};

inline QString Service::getClassValue(QString f_key) const
{
    const QMetaObject *l_object = metaObject();
    const int l_index = l_object->indexOfClassInfo(f_key.toStdString().c_str());
    QMetaClassInfo l_classInfo = l_object->classInfo(l_index);
    return QString::fromLatin1(l_classInfo.value());
}
