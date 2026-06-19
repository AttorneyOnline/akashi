#include "akashi/database_service.h"

#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimer>

namespace akashi {

DatabaseService::DatabaseService(const QString &f_data_root, QObject *parent) :
    QObject(parent),
    m_data_root(f_data_root)
{}

QString DatabaseService::serviceId() const
{
    return QStringLiteral("akashi.database");
}

ServiceVersion DatabaseService::serviceVersion() const
{
    return {1, 0, 0};
}

DatabaseService::~DatabaseService()
{
    for (const QString &l_name : std::as_const(m_connection_names)) {
        QSqlDatabase::database(l_name).close();
        QSqlDatabase::removeDatabase(l_name);
    }
}

bool DatabaseService::open(const QString &f_legacy_path)
{
    QDir().mkpath(m_data_root);
    const QString l_path = m_data_root + "/akashi.db";

    // A database from the legacy location is copied once, the original stays as a backup.
    if (!QFileInfo::exists(l_path) && !f_legacy_path.isEmpty() && QFileInfo::exists(f_legacy_path)) {
        QFile::copy(f_legacy_path, l_path);
        qInfo() << "Copied" << f_legacy_path << "to" << l_path;
    }

    QSqlDatabase l_database = QSqlDatabase::addDatabase("QSQLITE", "akashi_main");
    l_database.setDatabaseName(l_path);
    m_connection_names.append("akashi_main");
    if (!l_database.open()) {
        qCritical() << "Database error:" << l_database.lastError().text();
        return false;
    }
    applyPragmas(l_database);
    return true;
}

// Write-ahead logging keeps writes from blocking readers and cuts fsync cost;
// the busy timeout keeps future extra connections from failing outright.
void DatabaseService::applyPragmas(QSqlDatabase &f_database)
{
    QSqlQuery l_query(f_database);
    l_query.exec("PRAGMA journal_mode = WAL");
    l_query.exec("PRAGMA synchronous = NORMAL");
    l_query.exec("PRAGMA busy_timeout = 5000");
}

QSqlDatabase DatabaseService::database() const
{
    return QSqlDatabase::database("akashi_main");
}

QSqlDatabase DatabaseService::pluginDatabase(const QString &f_plugin_id)
{
    const QString l_name = "akashi_plugin_" + f_plugin_id;
    if (QSqlDatabase::contains(l_name)) {
        return QSqlDatabase::database(l_name);
    }

    QDir().mkpath(m_data_root + "/plugins");
    QSqlDatabase l_database = QSqlDatabase::addDatabase("QSQLITE", l_name);
    l_database.setDatabaseName(m_data_root + "/plugins/" + f_plugin_id + ".db");
    m_connection_names.append(l_name);
    if (!l_database.open()) {
        qCritical() << "Database error:" << l_database.lastError().text();
    }
    else {
        applyPragmas(l_database);
    }
    return l_database;
}

bool DatabaseService::applyMigration(QSqlDatabase f_database, int f_to_version, const std::function<bool(QSqlDatabase &)> &f_migration)
{
    if (!f_database.transaction()) {
        qCritical() << "Database error: could not start a transaction:" << f_database.lastError().text();
        return false;
    }

    if (!f_migration(f_database)) {
        qCritical() << "Database error: migration to version" << f_to_version << "failed:" << f_database.lastError().text();
        f_database.rollback();
        return false;
    }

    QSqlQuery l_version_query(f_database);
    l_version_query.exec("PRAGMA user_version = " + QString::number(f_to_version));
    if (!f_database.commit()) {
        qCritical() << "Database error: could not commit migration to version" << f_to_version;
        f_database.rollback();
        return false;
    }
    return true;
}

int DatabaseService::schemaVersion(QSqlDatabase f_database)
{
    QSqlQuery l_query(f_database);
    l_query.exec("PRAGMA user_version");
    return l_query.first() ? l_query.value(0).toInt() : 0;
}

void DatabaseService::scheduleMaintenance(const QTime &f_time, bool f_vacuum, const std::function<bool()> &f_busy_check)
{
    m_maintenance_time = f_time;
    m_maintenance_vacuum = f_vacuum;
    m_busy_check = f_busy_check;
    if (!f_time.isValid()) {
        return;
    }

    if (!m_maintenance_timer) {
        m_maintenance_timer = new QTimer(this);
        m_maintenance_timer->setSingleShot(true);
        connect(m_maintenance_timer, &QTimer::timeout, this, &DatabaseService::onMaintenanceDue);
    }
    m_maintenance_timer->start(msecsToNextOccurrence(f_time, QDateTime::currentDateTime()));
    qInfo() << "Database maintenance scheduled daily at" << f_time.toString("hh:mm");
}

void DatabaseService::onMaintenanceDue()
{
    // A busy server gets another try in half an hour.
    if (m_busy_check && m_busy_check()) {
        qInfo() << "Database maintenance postponed, the server is busy.";
        m_maintenance_timer->start(30 * 60 * 1000);
        return;
    }

    runMaintenance();
    Q_EMIT maintenanceTriggered();
    m_maintenance_timer->start(msecsToNextOccurrence(m_maintenance_time, QDateTime::currentDateTime()));
}

void DatabaseService::runMaintenance()
{
    for (const QString &l_name : std::as_const(m_connection_names)) {
        QSqlDatabase l_database = QSqlDatabase::database(l_name);
        if (!l_database.isOpen()) {
            continue;
        }

        QElapsedTimer l_stopwatch;
        l_stopwatch.start();
        QSqlQuery l_query(l_database);
        l_query.exec("PRAGMA optimize");
        l_query.exec("PRAGMA wal_checkpoint(TRUNCATE)");
        if (m_maintenance_vacuum) {
            l_query.exec("VACUUM");
        }
        qInfo() << "Database maintenance on" << l_database.databaseName() << "took" << l_stopwatch.elapsed() << "ms";
    }
}

qint64 DatabaseService::msecsToNextOccurrence(const QTime &f_time, const QDateTime &f_now)
{
    QDateTime l_next(f_now.date(), f_time);
    if (l_next <= f_now) {
        l_next = l_next.addDays(1);
    }
    return f_now.msecsTo(l_next);
}

} // namespace akashi
