#ifndef AKASHI_DATABASE_SERVICE_H
#define AKASHI_DATABASE_SERVICE_H

#include "akashi/service.h"
#include "akashi_core_export.h"

#include <QDateTime>
#include <QObject>
#include <QSqlDatabase>

#include <functional>

class QTimer;

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

    // Runs a migration inside a transaction and bumps the schema version on success.
    static bool applyMigration(QSqlDatabase f_database, int f_to_version, const std::function<bool(QSqlDatabase &)> &f_migration);

    // The schema version of a database, stored in PRAGMA user_version.
    static int schemaVersion(QSqlDatabase f_database);

    // Runs maintenance once a day at the given time; an invalid time disables it.
    // The busy check may postpone a run while the server has players.
    void scheduleMaintenance(const QTime &f_time, bool f_vacuum, const std::function<bool()> &f_busy_check = {});

    // Refreshes planner statistics and trims the write-ahead logs of every open
    // database, and compacts them too when vacuum is on.
    void runMaintenance();

    // How long until the clock next shows the given time.
    static qint64 msecsToNextOccurrence(const QTime &f_time, const QDateTime &f_now);

  private:
    static void applyPragmas(QSqlDatabase &f_database);
    void onMaintenanceDue();

    QString m_data_root;
    QStringList m_connection_names;
    QTimer *m_maintenance_timer = nullptr;
    QTime m_maintenance_time;
    bool m_maintenance_vacuum = false;
    std::function<bool()> m_busy_check;
};

} // namespace akashi

#endif // AKASHI_DATABASE_SERVICE_H
