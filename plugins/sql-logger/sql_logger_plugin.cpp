#include "sql_logger_plugin.h"

#include "akashi/config_store.h"
#include "akashi/filesystem_service.h"
#include "akashi/logging_categories.h"
#include "akashi/service_registry.h"
#include "core/log_service.h"
#include "writer_sql.h"

#include <QDebug>

QString SqlLoggerPlugin::id() const { return QStringLiteral("akashi.sql-logger"); }
akashi::ServiceVersion SqlLoggerPlugin::pluginVersion() const { return {1, 0, 0}; }

bool SqlLoggerPlugin::load(akashi::ServiceRegistry &services)
{
    auto l_fs = services.resolve<akashi::FileSystemService>(QStringLiteral("akashi.filesystem"));
    auto l_log = services.resolve<akashi::LogService>(QStringLiteral("akashi.log"));
    auto l_config = services.resolve<akashi::ConfigStore>(QStringLiteral("akashi.config"));

    if (!l_fs || !l_log || !l_config) {
        qCWarning(akashiLog) << "sql-logger: required services not available";
        return false;
    }

    const QString l_default_path = QStringLiteral("logs/logging.db");
    const QString l_config_name = QStringLiteral("plugins/") + id();
    l_config->declarePlugin(id(), {akashi::ConfigEntry(QStringLiteral("db_path"), l_default_path,
                                                       QStringLiteral("Path to the SQLite event log, relative to the server root."))});

    QString l_db_path = l_config->get<QString>(l_config_name, QStringLiteral("db_path"));
    auto l_resolved = l_fs->resolve(akashi::FileSystemService::Scope::System, l_db_path);
    if (!l_resolved) {
        qCWarning(akashiLog) << "sql-logger: path escapes server root:" << l_db_path;
        return false;
    }

    m_writer = std::make_shared<akashi::WriterSql>(*l_resolved, QStringLiteral("sql_logger_plugin"));
    l_log->registerWriter(m_writer, id());

    qCInfo(akashiLog).noquote() << "sql-logger: logging events to" << *l_resolved;
    return true;
}

void SqlLoggerPlugin::shutdown(akashi::ServiceRegistry &services)
{
    auto l_log = services.resolve<akashi::LogService>(QStringLiteral("akashi.log"));
    if (l_log)
        l_log->unregisterAll(id());

    m_writer.reset();
}
