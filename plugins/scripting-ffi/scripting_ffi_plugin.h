// AI-generated: written by Claude.
#pragma once

#include "akashi/plugin.h"

#include <QObject>
#include <QtPlugin>

#include <memory>

class ScriptingFfiService;

// Exposes the plugin SDK over a flat C table so hosts can run plugins
// written in languages that cannot touch Qt or C++.
class ScriptingFfiPlugin : public QObject, public akashi::IPlugin
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
    std::shared_ptr<ScriptingFfiService> m_service;
};
