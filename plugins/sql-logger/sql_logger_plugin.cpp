#include "sql_logger_plugin.h"

#include "akashi/config_store.h"
#include "akashi/database_service.h"
#include "akashi/logging_categories.h"
#include "akashi/service_registry.h"
#include "akashi/setting_notifier.h"
#include "core/log_service.h"
#include "writer_sql.h"

#include <QDebug>
#include <QSqlDatabase>

QString SqlLoggerPlugin::id() const { return QStringLiteral("akashi.sql-logger"); }
akashi::ServiceVersion SqlLoggerPlugin::pluginVersion() const { return {1, 0, 0}; }

bool SqlLoggerPlugin::load(akashi::ServiceRegistry &services)
{
    m_log = services.resolve<akashi::LogService>(QStringLiteral("akashi.log"));
    m_databases = services.resolve<akashi::DatabaseService>(QStringLiteral("akashi.database"));
    m_config = services.resolve<akashi::ConfigStore>(QStringLiteral("akashi.config"));

    if (!m_log || !m_databases || !m_config) {
        qCWarning(akashiLog) << "sql-logger: required services not available";
        return false;
    }

    // The writer only exists while the server's logging mode asks for a
    // database, and a mode change through /reload takes effect without a
    // plugin reload.
    applyLoggingMode(true);
    connect(m_config->notifier(QStringLiteral("config"), QStringLiteral("Options/logging")),
            &akashi::SettingNotifier::changed, this, &SqlLoggerPlugin::onLoggingModeChanged);
    return true;
}

void SqlLoggerPlugin::onLoggingModeChanged()
{
    applyLoggingMode(false);
}

void SqlLoggerPlugin::applyLoggingMode(bool f_initial)
{
    const QString l_mode = m_config->get<QString>(QStringLiteral("config"), QStringLiteral("Options/logging")).toLower();
    const bool l_wants_writer = l_mode == QStringLiteral("sql");

    if (l_wants_writer && !m_writer) {
        // The database service owns the file: it lands in the data folder
        // and takes part in scheduled maintenance and backups. The writer
        // runs on the log worker thread, so it opens its own connection to
        // that file - a Qt SQL connection must stay on its own thread.
        QSqlDatabase l_database = m_databases->pluginDatabase(id());
        if (!l_database.isOpen()) {
            qCWarning(akashiLog) << "sql-logger: could not open the plugin database";
            return;
        }
        m_writer = std::make_shared<akashi::WriterSql>(l_database.databaseName(), QStringLiteral("sql_logger_plugin"));
        m_log->registerWriter(m_writer, id());
        qCInfo(akashiLog).noquote() << "sql-logger: logging events to" << l_database.databaseName();
    }
    else if (!l_wants_writer && m_writer) {
        // Drop our reference first, then unregister: the writer's SQL
        // connection is affine to the log worker thread, so LogService must
        // hold the last reference and dispose it on that thread.
        m_writer.reset();
        m_log->unregisterAll(id());
        qCInfo(akashiLog).noquote() << QStringLiteral("sql-logger: logging is %1, the SQL writer retired").arg(l_mode);
    }
    else if (!l_wants_writer && f_initial) {
        qCInfo(akashiLog).noquote() << QStringLiteral("sql-logger: logging is %1, the SQL writer stays off until the mode is sql").arg(l_mode);
    }
}

void SqlLoggerPlugin::shutdown(akashi::ServiceRegistry &services)
{
    auto l_log = services.resolve<akashi::LogService>(QStringLiteral("akashi.log"));
    // Same ordering as the runtime retirement: release our reference before
    // unregistering so the worker thread disposes the SQL connection.
    m_writer.reset();
    if (l_log)
        l_log->unregisterAll(id());

    m_config.reset();
    m_databases.reset();
    m_log.reset();
}
