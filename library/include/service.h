#pragma once

#include "akashi_addon_global.h"

#include <QMap>
#include <QObject>
#include <QVariant>
#include <QVersionNumber>

class ServiceRegistry;

class AKASHI_ADDON_EXPORT Service : public QObject
{
    Q_OBJECT

  public:
    enum State
    {
        PENDING,
        OK,
        FAILED
    };
    Q_ENUM(State);

    Service(QObject *parent = nullptr) : QObject{parent} {};
    ~Service() = default;

    virtual void setServiceRegistry(ServiceRegistry *f_registry = nullptr) {};
    Service::State getState() { return m_state; };

    QString getServiceProperty(QString f_key) const
    {
        return m_service_properties.value(f_key, {});
    }

  protected:
    Service::State m_state = State::PENDING;
    QMap<QString, QString> m_service_properties;
    ServiceRegistry *m_registry;
};
