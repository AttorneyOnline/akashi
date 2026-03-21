#include "akashi/database_service.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>

namespace akashi {

DatabaseService::DatabaseService(const QString &f_data_root, QObject *parent) :
    QObject(parent),
    m_data_root(f_data_root)
{}

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
    return true;
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

} // namespace akashi
