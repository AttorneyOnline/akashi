#pragma once

#include "akashi/plugin.h"

#include <QObject>
#include <QtPlugin>

#include <memory>

class LuaScriptHost;

// The dependency plugin Lua plugins run on: registers the
// akashi.script-host.lua service the plugin manager hands .lua plugins to.
// Every Lua plugin gets its own interpreter state, and everything it
// registers carries the plugin's own id, so it lists, unloads and sweeps
// like any other plugin.
class LuaHostPlugin : public QObject, public akashi::IPlugin
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
    std::shared_ptr<LuaScriptHost> m_host;
};
