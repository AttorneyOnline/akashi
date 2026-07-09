#pragma once

#include "akashi/service.h"
#include "akashi_core_export.h"

#include <QObject>
#include <QSqlDatabase>

#include <functional>

namespace akashi {

// Owns the database connections. Plugins get their own file through pluginDatabase().
class AKASHI_CORE_EXPORT DatabaseService : public QObject, public IService
{
    Q_OBJECT

  public:
    explicit DatabaseService(const QString &f_data_root = QStringLiteral("data"), QObject *parent = nullptr);
    ~DatabaseService();

    QString serviceId() const override;
    ServiceVersion serviceVersion() const override;

    // Opens the server database, copying a database from the legacy location first if needed.
    bool open(const QString &f_legacy_path = QString());

    // The server database connection.
    QSqlDatabase database() const;

    // A plugin's own database, stored as plugins/<id>.db inside the data folder.
    QSqlDatabase pluginDatabase(const QString &f_plugin_id);

    // A read-only connection to an existing database for pure readers: an
    // empty source or "main" is the server database, any other name is that
    // plugin's database. Opened with SQLITE_OPEN_READONLY, so a write is
    // refused by the engine itself. Invalid when the file does not exist.
    QSqlDatabase reader(const QString &f_source);

    // Runs a migration inside a transaction and bumps the schema version on success.
    static bool applyMigration(QSqlDatabase f_database, int f_to_version, const std::function<bool(QSqlDatabase &)> &f_migration);

    // The schema version of a database, stored in PRAGMA user_version.
    static int schemaVersion(QSqlDatabase f_database);

    // Refreshes planner statistics and trims the write-ahead logs of every
    // open database, and compacts them too when vacuum is on. Timing lives
    // in the Scheduler; the server wires the two together.
    void runMaintenance(bool f_vacuum);

    // Runs maintenance and fires maintenanceTriggered so log writers
    // maintain too.
    void runMaintenanceNow(bool f_vacuum);

    // Copies every open database into backups/ inside the data folder,
    // keeping the newest f_keep copies each; returns how many were written.
    int runBackups(int f_keep);

    // One line per open database: path, size, write-ahead log size, schema version.
    QStringList overview() const;

  Q_SIGNALS:
    void maintenanceTriggered();

  private:
    static void applyPragmas(QSqlDatabase &f_database);

    QString m_data_root;
    QStringList m_connection_names;
    // Read-only connections stay out of m_connection_names: they are never
    // maintained or backed up (a read-only connection cannot be), only closed.
    QStringList m_reader_names;
};

} // namespace akashi
