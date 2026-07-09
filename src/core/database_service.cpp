#include "akashi/database_service.h"

#include "akashi/logging_categories.h"

#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QSqlError>
#include <QSqlQuery>

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
    for (const QString &l_name : std::as_const(m_reader_names)) {
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
        qCInfo(akashiDb) << "Copied" << f_legacy_path << "to" << l_path;
    }

    QSqlDatabase l_database = QSqlDatabase::addDatabase("QSQLITE", "akashi_main");
    l_database.setDatabaseName(l_path);
    m_connection_names.append("akashi_main");
    if (!l_database.open()) {
        qCCritical(akashiDb) << "Database error:" << l_database.lastError().text();
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
        qCCritical(akashiDb) << "Database error:" << l_database.lastError().text();
    }
    else {
        applyPragmas(l_database);
    }
    return l_database;
}

QSqlDatabase DatabaseService::reader(const QString &f_source)
{
    const QString l_key = (f_source.isEmpty() || f_source == QStringLiteral("main"))
                              ? QStringLiteral("main")
                              : f_source;
    const QString l_path = l_key == QStringLiteral("main")
                               ? m_data_root + "/akashi.db"
                               : m_data_root + "/plugins/" + l_key + ".db";
    if (!QFileInfo::exists(l_path)) {
        return {};
    }

    const QString l_name = "akashi_reader_" + l_key;
    if (QSqlDatabase::contains(l_name)) {
        return QSqlDatabase::database(l_name);
    }

    QSqlDatabase l_database = QSqlDatabase::addDatabase("QSQLITE", l_name);
    l_database.setDatabaseName(l_path);
    // Read-only at the engine level: any write is refused, and no PRAGMA can
    // re-enable it because the file handle itself is read-only.
    l_database.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
    m_reader_names.append(l_name);
    if (!l_database.open()) {
        qCWarning(akashiDb) << "Read-only open of" << l_path << "failed:" << l_database.lastError().text();
    }
    return l_database;
}

bool DatabaseService::applyMigration(QSqlDatabase f_database, int f_to_version, const std::function<bool(QSqlDatabase &)> &f_migration)
{
    if (!f_database.transaction()) {
        qCCritical(akashiDb) << "Database error: could not start a transaction:" << f_database.lastError().text();
        return false;
    }

    if (!f_migration(f_database)) {
        qCCritical(akashiDb) << "Database error: migration to version" << f_to_version << "failed:" << f_database.lastError().text();
        f_database.rollback();
        return false;
    }

    QSqlQuery l_version_query(f_database);
    l_version_query.exec("PRAGMA user_version = " + QString::number(f_to_version));
    if (!f_database.commit()) {
        qCCritical(akashiDb) << "Database error: could not commit migration to version" << f_to_version;
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

void DatabaseService::runMaintenanceNow(bool f_vacuum)
{
    runMaintenance(f_vacuum);
    Q_EMIT maintenanceTriggered();
}

void DatabaseService::runMaintenance(bool f_vacuum)
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
        if (f_vacuum) {
            l_query.exec("VACUUM");
        }
        qCInfo(akashiDb) << "Database maintenance on" << l_database.databaseName() << "took" << l_stopwatch.elapsed() << "ms";
    }
}

int DatabaseService::runBackups(int f_keep)
{
    const int l_keep = qMax(1, f_keep);
    const QString l_backup_root = m_data_root + "/backups";
    QDir().mkpath(l_backup_root);
    const QString l_stamp = QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
    int l_written = 0;
    for (const QString &l_name : std::as_const(m_connection_names)) {
        QSqlDatabase l_database = QSqlDatabase::database(l_name);
        if (!l_database.isOpen()) {
            continue;
        }

        // VACUUM INTO writes a compacted, transactionally consistent copy
        // without blocking the live connection.
        const QString l_base = QFileInfo(l_database.databaseName()).completeBaseName();
        const QString l_target = l_backup_root + "/" + l_base + "-" + l_stamp + ".db";
        if (QFileInfo::exists(l_target)) {
            QFile::remove(l_target);
        }
        QString l_escaped = l_target;
        l_escaped.replace("'", "''");
        QSqlQuery l_query(l_database);
        if (!l_query.exec("VACUUM INTO '" + l_escaped + "'")) {
            qCWarning(akashiDb) << "Backup of" << l_database.databaseName() << "failed:" << l_query.lastError().text();
            continue;
        }
        l_written++;

        // The stamp sorts chronologically, so the oldest copies go first.
        QDir l_dir(l_backup_root);
        QStringList l_backups = l_dir.entryList({l_base + "-*.db"}, QDir::Files, QDir::Name);
        while (l_backups.size() > l_keep) {
            l_dir.remove(l_backups.takeFirst());
        }
    }
    qCInfo(akashiDb) << "Backed up" << l_written << "database(s) to" << l_backup_root;
    return l_written;
}

QStringList DatabaseService::overview() const
{
    const QLocale l_locale;
    QStringList l_lines;
    for (const QString &l_name : std::as_const(m_connection_names)) {
        QSqlDatabase l_database = QSqlDatabase::database(l_name);
        if (!l_database.isOpen()) {
            continue;
        }
        const QFileInfo l_info(l_database.databaseName());
        const QFileInfo l_wal(l_database.databaseName() + "-wal");
        l_lines.append(QStringLiteral("%1 | %2 | WAL %3 | schema v%4")
                           .arg(l_info.filePath(),
                                l_locale.formattedDataSize(l_info.size()),
                                l_wal.exists() ? l_locale.formattedDataSize(l_wal.size()) : QStringLiteral("0 B"))
                           .arg(schemaVersion(l_database)));
    }
    return l_lines;
}

} // namespace akashi
