#pragma once

#include "akashi_addon_global.h"

#include <QLoggingCategory>
#include <QMap>
#include <QObject>
#include <QVariant>

class ServiceRegistry;

Q_DECLARE_EXPORTED_LOGGING_CATEGORY(akashiService, AKASHI_ADDON_EXPORT)
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

    virtual void setServiceRegistry(ServiceRegistry *f_registry = nullptr);
    void setState(Service::State f_state);
    Service::State getState();

    QString getServiceProperty(QString f_key) const;

  protected:
    Service::State m_state = State::PENDING;
    QMap<QString, QString> m_service_properties;
    ServiceRegistry *m_registry;
};
