#pragma once

#include "akashi/plugin.h"
#include "medieval_parser.h"

#include <QObject>
#include <QtPlugin>

#include <memory>

class MedievalSpeakPlugin : public QObject, public akashi::IPlugin
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
    std::unique_ptr<MedievalParser> m_parser;
};
