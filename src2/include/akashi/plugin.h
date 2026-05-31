#pragma once

#include "akashi/service.h"

#include <QObject>
#include <QString>

namespace akashi {

class ServiceRegistry;

class IPlugin
{
public:
    virtual ~IPlugin() = default;

    virtual QString id() const = 0;
    virtual ServiceVersion pluginVersion() const = 0;

    virtual bool load(ServiceRegistry &services) = 0;
    virtual bool init(ServiceRegistry &services) { Q_UNUSED(services); return true; }
    virtual void started(ServiceRegistry &services) { Q_UNUSED(services); }
    virtual void shutdown(ServiceRegistry &services) = 0;
};

} // namespace akashi

#define AkashiPlugin_iid "org.akashi.IPlugin/1"
Q_DECLARE_INTERFACE(akashi::IPlugin, AkashiPlugin_iid)

