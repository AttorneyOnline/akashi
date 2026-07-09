#pragma once

#include "akashi/log_writer.h"
#include "akashi/plugin.h"

#include <QObject>
#include <QtPlugin>

#include <memory>

namespace akashi {
class WriterSql;
class LogService;
class DatabaseService;
class ConfigStore;
}

class SqlLoggerPlugin : public QObject, public akashi::IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID AkashiPlugin_iid FILE "plugin.json")
    Q_INTERFACES(akashi::IPlugin)

  public:
    QString id() const override;
    akashi::ServiceVersion pluginVersion() const override;

    bool load(akashi::ServiceRegistry &services) override;
    void shutdown(akashi::ServiceRegistry &services) override;

  private Q_SLOTS:
    // A /reload changed the logging setting; re-evaluate the writer.
    void onLoggingModeChanged();

  private:
    // Registers or retires the writer to match the logging mode; only the
    // sql mode puts events into the database.
    void applyLoggingMode(bool f_initial);

    std::shared_ptr<akashi::WriterSql> m_writer;
    std::shared_ptr<akashi::LogService> m_log;
    std::shared_ptr<akashi::DatabaseService> m_databases;
    std::shared_ptr<akashi::ConfigStore> m_config;
};
