#pragma once

#include <QMap>
#include <QObject>
#include <QVariant>
#include <QVersionNumber>

#include "akashi_core_global.h"

class ServiceRegistry;

class AKASHI_CORE_EXPORT Service : public QObject
{
    Q_OBJECT

  public:
      Service(ServiceRegistry *f_registry, QObject *parent = nullptr)
          : QObject{parent}
          , m_registry{f_registry}
          , m_service_properties{{"author", "Salanto"},
                                 {"version", "1.0.0"},
                                 {"identifier", "akashi.core.service"}} {};
      ~Service() = default;

      QString getServiceProperty(QString f_key) const
      {
          return m_service_properties.value(f_key, {});
      }

  protected:
      QMap<QString, QString> m_service_properties;
      ServiceRegistry *m_registry;
};
