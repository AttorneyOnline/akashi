#pragma once

#include "akashi/plugin.h"

#include <QObject>
#include <QtPlugin>

#include <memory>

class PythonScriptHost;

// The dependency plugin Python plugins run on: registers the
// akashi.script-host.python service the plugin manager hands .py plugins
// to. Every Python plugin runs in its own module namespace of one embedded
// interpreter, and everything it registers carries the plugin's own id.
class PythonHostPlugin : public QObject, public akashi::IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID AkashiPlugin_iid FILE "plugin.json")
    Q_INTERFACES(akashi::IPlugin)

  public:
    QString id() const override;
    akashi::ServiceVersion pluginVersion() const override;

    bool load(akashi::ServiceRegistry &services) override;
    void shutdown(akashi::ServiceRegistry &services) override;

  private:
    std::shared_ptr<PythonScriptHost> m_host;
};
