#pragma once

#include <QObject>

#include "service.h"

class IPlugin : public Service
{
    Q_OBJECT

  public:
    virtual ~IPlugin() = default;
    virtual bool initialize(ServiceRegistry *f_registry) = 0;
    virtual void shutdown() = 0;
};

Q_DECLARE_INTERFACE(IPlugin, "com.akashi.IPlugin/1.0")
