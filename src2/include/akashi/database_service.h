#ifndef AKASHI_DATABASE_SERVICE_H
#define AKASHI_DATABASE_SERVICE_H

#include "akashi_core_export.h"

#include <QObject>
#include <QSqlDatabase>

#include <functional>

namespace akashi {

// Owns the database connections. Plugins get their own file through pluginDatabase().
class AKASHI_CORE_EXPORT DatabaseService : public QObject
{
    Q_OBJECT

  public:
    explicit DatabaseService(const QString &f_data_root = QStringLiteral("data"), QObject *parent = nullptr);
    ~DatabaseService();

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

  private:
    QString m_data_root;
    QStringList m_connection_names;
};

} // namespace akashi

#endif // AKASHI_DATABASE_SERVICE_H
