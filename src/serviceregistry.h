#pragma once

#include <QMap>
#include <QObject>
#include <qdebug.h>
#include <type_traits>

#include <service.h>

class ServiceRegistry : public QObject
{
    Q_OBJECT

  public:
    explicit ServiceRegistry(QObject *f_parent = nullptr);

    bool hasService(QString identifier);
    void registerService(Service *f_pointer);
    void removeService(QString f_identifier);
    std::optional<QString> getServiceInfo(QString f_identifier, QString f_key);

    template <typename T>
    inline T *getService(QString f_identifier)
        requires std::is_base_of_v<Service, T>
    {
        Service *l_pointer = m_services.value(f_identifier, nullptr);

        if (!l_pointer) {
            qDebug() << QString("Failed to find service identifier %1.").arg(f_identifier);
            return nullptr;
        }

        return dynamic_cast<T *>(l_pointer);
    }

  private:
    QString getClassInfo(const QMetaObject *f_metaObject, QString f_key) const;

    QMap<QString, Service *> m_services;
};
