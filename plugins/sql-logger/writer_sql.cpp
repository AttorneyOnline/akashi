#include "writer_sql.h"

#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QSqlError>

namespace akashi {

WriterSql::WriterSql(const QString &f_db_path, const QString &f_connection_name) :
    m_db_path(f_db_path),
    m_connection_name(f_connection_name.isEmpty() ? QStringLiteral("akashi_log_writer") : f_connection_name)
{}

WriterSql::~WriterSql()
{
    m_insert_type.reset();
    m_select_type.reset();
    m_insert_identity.reset();
    m_select_identity.reset();
    m_insert_connection.reset();
    m_insert_event.reset();

    if (QSqlDatabase::contains(m_connection_name)) {
        {
            QSqlDatabase l_db = QSqlDatabase::database(m_connection_name);
            if (m_in_transaction) {
                l_db.commit();
            }
            l_db.close();
        }
        QSqlDatabase::removeDatabase(m_connection_name);
    }
}

QString WriterSql::writerId() const
{
    return QStringLiteral("akashi.writer.sql");
}

void WriterSql::write(const LogEvent &f_event)
{
    if (!ensureOpen()) {
        return;
    }
    beginIfNeeded();

    if (f_event.type == QStringLiteral("connect")) {
        writeConnection(f_event);
    }
    else {
        writeEvent(f_event);
    }
}

void WriterSql::writeConnection(const LogEvent &f_event)
{
    int l_identity_id = 0;
    if (!f_event.ipid.isEmpty()) {
        l_identity_id = identityId(f_event.ipid, f_event.hwid);
    }

    m_insert_connection->bindValue(0, f_event.timestamp);
    m_insert_connection->bindValue(1, l_identity_id > 0 ? QVariant(l_identity_id) : QVariant());
    m_insert_connection->bindValue(2, f_event.ipid.isEmpty() ? QVariant() : f_event.ipid);
    m_insert_connection->bindValue(3, f_event.hwid.isEmpty() ? QVariant() : f_event.hwid);
    m_insert_connection->bindValue(4, f_event.client_id.isEmpty() ? QVariant() : f_event.client_id);
    m_insert_connection->bindValue(5, f_event.target_ipid.isEmpty() ? QVariant() : f_event.target_ipid);
    m_insert_connection->exec();
}

void WriterSql::writeEvent(const LogEvent &f_event)
{
    int l_type_id = typeId(f_event.type);
    int l_identity_id = 0;
    if (!f_event.ipid.isEmpty()) {
        l_identity_id = identityId(f_event.ipid, f_event.hwid);
    }

    m_insert_event->bindValue(0, f_event.timestamp);
    m_insert_event->bindValue(1, l_type_id);
    m_insert_event->bindValue(2, l_identity_id > 0 ? QVariant(l_identity_id) : QVariant());
    m_insert_event->bindValue(3, f_event.area.isEmpty() ? QVariant() : f_event.area);
    m_insert_event->bindValue(4, f_event.ipid.isEmpty() ? QVariant() : f_event.ipid);
    m_insert_event->bindValue(5, f_event.hwid.isEmpty() ? QVariant() : f_event.hwid);
    m_insert_event->bindValue(6, f_event.client_id.isEmpty() ? QVariant() : f_event.client_id);
    m_insert_event->bindValue(7, f_event.char_name.isEmpty() ? QVariant() : f_event.char_name);
    m_insert_event->bindValue(8, f_event.ooc_name.isEmpty() ? QVariant() : f_event.ooc_name);
    m_insert_event->bindValue(9, f_event.message.isEmpty() ? QVariant() : f_event.message);
    m_insert_event->bindValue(10, f_event.args.isEmpty() ? QVariant() : f_event.args);
    m_insert_event->bindValue(11, f_event.moderator.isEmpty() ? QVariant() : f_event.moderator);
    m_insert_event->bindValue(12, f_event.target_ipid.isEmpty() ? QVariant() : f_event.target_ipid);
    m_insert_event->bindValue(13, f_event.duration.isEmpty() ? QVariant() : f_event.duration);
    m_insert_event->bindValue(14, f_event.success ? 1 : 0);
    m_insert_event->exec();
}

void WriterSql::flush()
{
    if (!m_in_transaction) {
        return;
    }
    QSqlDatabase l_db = QSqlDatabase::database(m_connection_name);
    l_db.commit();
    m_in_transaction = false;
}

void WriterSql::maintenance()
{
    if (!m_opened) {
        return;
    }
    flush();

    QSqlDatabase l_db = QSqlDatabase::database(m_connection_name);
    QElapsedTimer l_stopwatch;
    l_stopwatch.start();

    QSqlQuery l_query(l_db);
    l_query.exec(QStringLiteral("PRAGMA optimize"));
    l_query.exec(QStringLiteral("PRAGMA wal_checkpoint(TRUNCATE)"));
    l_query.exec(QStringLiteral("ANALYZE"));
    qInfo() << "WriterSql: maintenance on" << m_db_path << "took" << l_stopwatch.elapsed() << "ms";
}

void WriterSql::beginIfNeeded()
{
    if (!m_in_transaction) {
        QSqlDatabase l_db = QSqlDatabase::database(m_connection_name);
        l_db.transaction();
        m_in_transaction = true;
    }
}

bool WriterSql::ensureOpen()
{
    if (m_opened) {
        return true;
    }

    QDir().mkpath(QFileInfo(m_db_path).absolutePath());

    QSqlDatabase l_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connection_name);
    l_db.setDatabaseName(m_db_path);
    if (!l_db.open()) {
        qCritical() << "WriterSql: cannot open" << m_db_path << l_db.lastError().text();
        return false;
    }

    QSqlQuery l_pragma(l_db);
    l_pragma.exec(QStringLiteral("PRAGMA journal_mode = WAL"));
    l_pragma.exec(QStringLiteral("PRAGMA synchronous = NORMAL"));
    l_pragma.exec(QStringLiteral("PRAGMA busy_timeout = 5000"));

    migrate(l_db);
    loadCaches(l_db);

    m_insert_type = std::make_unique<QSqlQuery>(l_db);
    m_insert_type->prepare(QStringLiteral("INSERT OR IGNORE INTO event_types(name) VALUES(?)"));

    m_select_type = std::make_unique<QSqlQuery>(l_db);
    m_select_type->prepare(QStringLiteral("SELECT id FROM event_types WHERE name = ?"));

    m_insert_identity = std::make_unique<QSqlQuery>(l_db);
    m_insert_identity->prepare(QStringLiteral("INSERT OR IGNORE INTO identities(ipid, hwid) VALUES(?, ?)"));

    m_select_identity = std::make_unique<QSqlQuery>(l_db);
    m_select_identity->prepare(QStringLiteral("SELECT id FROM identities WHERE ipid = ? AND hwid = ?"));

    m_insert_connection = std::make_unique<QSqlQuery>(l_db);
    m_insert_connection->prepare(QStringLiteral(
        "INSERT INTO connections(timestamp, identity_id, ipid, hwid, client_id, target_ipid) "
        "VALUES(?, ?, ?, ?, ?, ?)"));

    m_insert_event = std::make_unique<QSqlQuery>(l_db);
    m_insert_event->prepare(QStringLiteral(
        "INSERT INTO events(timestamp, type_id, identity_id, area, ipid, hwid, client_id, "
        "char_name, ooc_name, message, args, moderator, target_ipid, duration, success) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));

    m_opened = true;
    return true;
}

void WriterSql::migrate(QSqlDatabase &f_db)
{
    QSqlQuery l_query(f_db);
    l_query.exec(QStringLiteral("PRAGMA user_version"));
    int l_version = l_query.first() ? l_query.value(0).toInt() : 0;

    if (l_version >= 1) {
        return;
    }

    f_db.transaction();

    l_query.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS identities ("
        "id INTEGER PRIMARY KEY, "
        "ipid TEXT NOT NULL, "
        "hwid TEXT NOT NULL DEFAULT '', "
        "UNIQUE(ipid, hwid))"));

    l_query.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS connections ("
        "id INTEGER PRIMARY KEY, "
        "timestamp INTEGER NOT NULL, "
        "identity_id INTEGER, "
        "ipid TEXT, "
        "hwid TEXT, "
        "client_id TEXT, "
        "target_ipid TEXT)"));

    l_query.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS event_types ("
        "id INTEGER PRIMARY KEY, "
        "name TEXT NOT NULL UNIQUE)"));

    l_query.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS events ("
        "id INTEGER PRIMARY KEY, "
        "timestamp INTEGER NOT NULL, "
        "type_id INTEGER NOT NULL, "
        "identity_id INTEGER, "
        "area TEXT, "
        "ipid TEXT, "
        "hwid TEXT, "
        "client_id TEXT, "
        "char_name TEXT, "
        "ooc_name TEXT, "
        "message TEXT, "
        "args TEXT, "
        "moderator TEXT, "
        "target_ipid TEXT, "
        "duration TEXT, "
        "success INTEGER NOT NULL DEFAULT 1)"));

    l_query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_connections_identity ON connections(identity_id, timestamp)"));
    l_query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_connections_ipid ON connections(ipid, timestamp)"));
    l_query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_connections_hwid ON connections(hwid, timestamp)"));

    l_query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_events_identity ON events(identity_id, timestamp)"));
    l_query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_events_ipid ON events(ipid, timestamp)"));
    l_query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_events_hwid ON events(hwid, timestamp)"));
    l_query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_events_area ON events(area, timestamp)"));
    l_query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_events_type ON events(type_id, timestamp)"));
    l_query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_events_moderator ON events(moderator, timestamp)"));
    l_query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_events_target ON events(target_ipid, timestamp)"));

    l_query.exec(QStringLiteral("PRAGMA user_version = 1"));

    f_db.commit();
}

void WriterSql::loadCaches(QSqlDatabase &f_db)
{
    QSqlQuery l_query(f_db);

    l_query.exec(QStringLiteral("SELECT id, name FROM event_types"));
    while (l_query.next()) {
        m_type_cache.insert(l_query.value(1).toString(), l_query.value(0).toInt());
    }

    l_query.exec(QStringLiteral("SELECT id, ipid, hwid FROM identities"));
    while (l_query.next()) {
        m_identity_cache.insert({l_query.value(1).toString(), l_query.value(2).toString()},
                                l_query.value(0).toInt());
    }
}

int WriterSql::typeId(const QString &f_name)
{
    auto l_it = m_type_cache.constFind(f_name);
    if (l_it != m_type_cache.constEnd()) {
        return l_it.value();
    }

    m_insert_type->bindValue(0, f_name);
    m_insert_type->exec();

    m_select_type->bindValue(0, f_name);
    m_select_type->exec();
    m_select_type->first();
    int l_id = m_select_type->value(0).toInt();
    m_type_cache.insert(f_name, l_id);
    return l_id;
}

int WriterSql::identityId(const QString &f_ipid, const QString &f_hwid)
{
    const auto l_key = qMakePair(f_ipid, f_hwid);
    auto l_it = m_identity_cache.constFind(l_key);
    if (l_it != m_identity_cache.constEnd()) {
        return l_it.value();
    }

    m_insert_identity->bindValue(0, f_ipid);
    m_insert_identity->bindValue(1, f_hwid);
    m_insert_identity->exec();

    m_select_identity->bindValue(0, f_ipid);
    m_select_identity->bindValue(1, f_hwid);
    m_select_identity->exec();
    m_select_identity->first();
    int l_id = m_select_identity->value(0).toInt();
    m_identity_cache.insert(l_key, l_id);
    return l_id;
}

} // namespace akashi
