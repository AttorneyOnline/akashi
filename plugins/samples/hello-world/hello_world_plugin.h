#pragma once

#include "akashi/plugin.h"

#include <QObject>
#include <QtPlugin>

namespace akashi {
class CommandContext;
}

class HelloWorldPlugin : public QObject, public akashi::IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID AkashiPlugin_iid FILE "plugin.json")
    Q_INTERFACES(akashi::IPlugin)

  public:
    QString id() const override;
    akashi::ServiceVersion pluginVersion() const override;

    bool load(akashi::ServiceRegistry &services) override;
    void shutdown(akashi::ServiceRegistry &services) override;

    // The command handler this plugin exports to the registry.
    static void cmdHello(akashi::CommandContext &f_context);
};
