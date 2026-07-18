// AI-generated: written by Claude.
#include "akashi/log_event.h"
#include "writer_sql.h"

#include <QDir>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

namespace tests {
namespace unittests {

class tst_WriterSql : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void createsDatabaseOnFirstWrite();
    void connectEventGoesToConnectionsTable();
    void nonConnectEventGoesToEventsTable();
    void identitySharedBetweenTables();
    void roundTripEvent();
    void flushCommitsTransaction();
    void batchWriteThenFlush();
    void ipidAndHwidDenormalizedOnEvents();
    void eventWithoutHwidKeepsItsIdentity();
    void ipidAndHwidDenormalizedOnConnections();
    void emptyFieldsStoreAsNull();
    void successFieldStoredCorrectly();
    void schemaIsWalMode();
    void multipleFlushesAreIdempotent();
    void reopenPreservesData();
    void ipidIndexExists();
    void failedInsertWarnsInsteadOfDroppingSilently();
};

static QSqlDatabase openReadOnly(const QString &f_path)
{
    static int l_counter = 0;
    QString l_name = QStringLiteral("test_reader_%1").arg(l_counter++);
    QSqlDatabase l_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), l_name);
    l_db.setDatabaseName(f_path);
    l_db.open();
    return l_db;
}

static void closeReadOnly(QSqlDatabase &f_db)
{
    QString l_name = f_db.connectionName();
    f_db.close();
    f_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(l_name);
}

void tst_WriterSql::createsDatabaseOnFirstWrite()
{
    QTemporaryDir l_dir;
    QString l_path = l_dir.path() + "/logs/events.db";

    {
        akashi::WriterSql l_writer(l_path);
        QCOMPARE(l_writer.writerId(), QStringLiteral("akashi.writer.sql"));

        akashi::LogEvent l_event;
        l_event.timestamp = 1000;
        l_event.type = akashi::log_type::IC;
        l_event.area = QStringLiteral("Lobby");
        l_writer.write(l_event);
        l_writer.flush();
    }

    QVERIFY(QFileInfo::exists(l_path));
}

void tst_WriterSql::connectEventGoesToConnectionsTable()
{
    QTemporaryDir l_dir;
    QString l_path = l_dir.path() + "/events.db";

    {
        akashi::WriterSql l_writer(l_path);
        akashi::LogEvent l_event;
        l_event.timestamp = 1000;
        l_event.type = akashi::log_type::Connect;
        l_event.ipid = QStringLiteral("abcd1234");
        l_event.hwid = QStringLiteral("hw-001");
        l_event.client_id = QStringLiteral("7");
        l_event.target_ipid = QStringLiteral("192.168.1.1");
        l_writer.write(l_event);
        l_writer.flush();
    }

    QSqlDatabase l_db = openReadOnly(l_path);
    QSqlQuery l_query(l_db);

    l_query.exec(QStringLiteral("SELECT COUNT(*) FROM connections"));
    l_query.first();
    QCOMPARE(l_query.value(0).toInt(), 1);

    l_query.exec(QStringLiteral("SELECT COUNT(*) FROM events"));
    l_query.first();
    QCOMPARE(l_query.value(0).toInt(), 0);

    l_query.exec(QStringLiteral("SELECT timestamp, ipid, hwid, client_id, target_ipid FROM connections"));
    l_query.first();
    QCOMPARE(l_query.value(0).toLongLong(), 1000LL);
    QCOMPARE(l_query.value(1).toString(), QStringLiteral("abcd1234"));
    QCOMPARE(l_query.value(2).toString(), QStringLiteral("hw-001"));
    QCOMPARE(l_query.value(3).toString(), QStringLiteral("7"));
    QCOMPARE(l_query.value(4).toString(), QStringLiteral("192.168.1.1"));

    closeReadOnly(l_db);
}

void tst_WriterSql::nonConnectEventGoesToEventsTable()
{
    QTemporaryDir l_dir;
    QString l_path = l_dir.path() + "/events.db";

    {
        akashi::WriterSql l_writer(l_path);
        akashi::LogEvent l_event;
        l_event.timestamp = 1000;
        l_event.type = akashi::log_type::IC;
        l_event.area = QStringLiteral("Lobby");
        l_event.message = QStringLiteral("Hello");
        l_writer.write(l_event);
        l_writer.flush();
    }

    QSqlDatabase l_db = openReadOnly(l_path);
    QSqlQuery l_query(l_db);

    l_query.exec(QStringLiteral("SELECT COUNT(*) FROM events"));
    l_query.first();
    QCOMPARE(l_query.value(0).toInt(), 1);

    l_query.exec(QStringLiteral("SELECT COUNT(*) FROM connections"));
    l_query.first();
    QCOMPARE(l_query.value(0).toInt(), 0);

    closeReadOnly(l_db);
}

void tst_WriterSql::identitySharedBetweenTables()
{
    QTemporaryDir l_dir;
    QString l_path = l_dir.path() + "/events.db";

    {
        akashi::WriterSql l_writer(l_path);

        akashi::LogEvent l_connect;
        l_connect.timestamp = 1000;
        l_connect.type = akashi::log_type::Connect;
        l_connect.ipid = QStringLiteral("abcd1234");
        l_connect.hwid = QStringLiteral("hw-001");
        l_writer.write(l_connect);

        akashi::LogEvent l_ic;
        l_ic.timestamp = 2000;
        l_ic.type = akashi::log_type::IC;
        l_ic.ipid = QStringLiteral("abcd1234");
        l_ic.hwid = QStringLiteral("hw-001");
        l_ic.area = QStringLiteral("Lobby");
        l_ic.message = QStringLiteral("Hello");
        l_writer.write(l_ic);

        l_writer.flush();
    }

    QSqlDatabase l_db = openReadOnly(l_path);
    QSqlQuery l_query(l_db);

    l_query.exec(QStringLiteral("SELECT COUNT(*) FROM identities"));
    l_query.first();
    QCOMPARE(l_query.value(0).toInt(), 1);

    l_query.exec(QStringLiteral("SELECT c.identity_id, e.identity_id FROM connections c, events e"));
    l_query.first();
    int l_conn_id = l_query.value(0).toInt();
    int l_event_id = l_query.value(1).toInt();
    QCOMPARE(l_conn_id, l_event_id);
    QVERIFY(l_conn_id > 0);

    closeReadOnly(l_db);
}

void tst_WriterSql::roundTripEvent()
{
    QTemporaryDir l_dir;
    QString l_path = l_dir.path() + "/events.db";

    {
        akashi::WriterSql l_writer(l_path);
        akashi::LogEvent l_event;
        l_event.timestamp = 1719300000000;
        l_event.type = akashi::log_type::Ban;
        l_event.area = QStringLiteral("Courtroom");
        l_event.ipid = QStringLiteral("beef1234");
        l_event.hwid = QStringLiteral("hw-99");
        l_event.char_name = QStringLiteral("Edgeworth");
        l_event.ooc_name = QStringLiteral("Edge");
        l_event.client_id = QStringLiteral("42");
        l_event.message = QStringLiteral("spamming");
        l_event.moderator = QStringLiteral("Admin");
        l_event.target_ipid = QStringLiteral("dead5678");
        l_event.duration = QStringLiteral("24h");
        l_event.success = true;
        l_writer.write(l_event);
        l_writer.flush();
    }

    QSqlDatabase l_db = openReadOnly(l_path);
    QSqlQuery l_query(l_db);
    l_query.exec(QStringLiteral(
        "SELECT e.timestamp, t.name, e.area, e.ipid, e.hwid, e.char_name, e.ooc_name, "
        "e.client_id, e.message, e.moderator, e.target_ipid, e.duration, e.success "
        "FROM events e "
        "JOIN event_types t ON e.type_id = t.id"));
    QVERIFY(l_query.first());
    QCOMPARE(l_query.value(0).toLongLong(), 1719300000000LL);
    QCOMPARE(l_query.value(1).toString(), QStringLiteral("ban"));
    QCOMPARE(l_query.value(2).toString(), QStringLiteral("Courtroom"));
    QCOMPARE(l_query.value(3).toString(), QStringLiteral("beef1234"));
    QCOMPARE(l_query.value(4).toString(), QStringLiteral("hw-99"));
    QCOMPARE(l_query.value(5).toString(), QStringLiteral("Edgeworth"));
    QCOMPARE(l_query.value(6).toString(), QStringLiteral("Edge"));
    QCOMPARE(l_query.value(7).toString(), QStringLiteral("42"));
    QCOMPARE(l_query.value(8).toString(), QStringLiteral("spamming"));
    QCOMPARE(l_query.value(9).toString(), QStringLiteral("Admin"));
    QCOMPARE(l_query.value(10).toString(), QStringLiteral("dead5678"));
    QCOMPARE(l_query.value(11).toString(), QStringLiteral("24h"));
    QCOMPARE(l_query.value(12).toInt(), 1);
    closeReadOnly(l_db);
}

void tst_WriterSql::flushCommitsTransaction()
{
    QTemporaryDir l_dir;
    QString l_path = l_dir.path() + "/events.db";

    akashi::WriterSql l_writer(l_path);
    akashi::LogEvent l_event;
    l_event.timestamp = 1000;
    l_event.type = akashi::log_type::IC;
    l_event.message = QStringLiteral("before flush");
    l_writer.write(l_event);
    l_writer.flush();

    QSqlDatabase l_db = openReadOnly(l_path);
    QSqlQuery l_query(l_db);
    l_query.exec(QStringLiteral("SELECT COUNT(*) FROM events"));
    l_query.first();
    QCOMPARE(l_query.value(0).toInt(), 1);
    closeReadOnly(l_db);
}

void tst_WriterSql::batchWriteThenFlush()
{
    QTemporaryDir l_dir;
    QString l_path = l_dir.path() + "/events.db";

    {
        akashi::WriterSql l_writer(l_path);
        for (int i = 0; i < 100; ++i) {
            akashi::LogEvent l_event;
            l_event.timestamp = 1000 + i;
            l_event.type = akashi::log_type::IC;
            l_event.area = QStringLiteral("Lobby");
            l_event.ipid = QStringLiteral("abc");
            l_event.message = QStringLiteral("msg %1").arg(i);
            l_writer.write(l_event);
        }
        l_writer.flush();
    }

    QSqlDatabase l_db = openReadOnly(l_path);
    QSqlQuery l_query(l_db);
    l_query.exec(QStringLiteral("SELECT COUNT(*) FROM events"));
    l_query.first();
    QCOMPARE(l_query.value(0).toInt(), 100);
    closeReadOnly(l_db);
}

void tst_WriterSql::ipidAndHwidDenormalizedOnEvents()
{
    QTemporaryDir l_dir;
    QString l_path = l_dir.path() + "/events.db";

    {
        akashi::WriterSql l_writer(l_path);
        akashi::LogEvent l_event;
        l_event.timestamp = 1000;
        l_event.type = akashi::log_type::IC;
        l_event.ipid = QStringLiteral("abcd1234");
        l_event.hwid = QStringLiteral("hw-001");
        l_writer.write(l_event);
        l_writer.flush();
    }

    QSqlDatabase l_db = openReadOnly(l_path);
    QSqlQuery l_query(l_db);
    l_query.exec(QStringLiteral("SELECT ipid, hwid, identity_id FROM events"));
    l_query.first();
    QCOMPARE(l_query.value(0).toString(), QStringLiteral("abcd1234"));
    QCOMPARE(l_query.value(1).toString(), QStringLiteral("hw-001"));
    QVERIFY(l_query.value(2).toInt() > 0);
    closeReadOnly(l_db);
}

void tst_WriterSql::eventWithoutHwidKeepsItsIdentity()
{
    QTemporaryDir l_dir;
    QString l_path = l_dir.path() + "/events.db";

    {
        akashi::WriterSql l_writer(l_path);
        // Most game events carry an ipid but no hardware id at all; the
        // identity must still resolve instead of failing on a NULL bind.
        akashi::LogEvent l_event;
        l_event.timestamp = 1000;
        l_event.type = akashi::log_type::IC;
        l_event.ipid = QStringLiteral("abcd1234");
        l_event.message = QStringLiteral("Hello");
        l_writer.write(l_event);
        l_writer.flush();
    }

    QSqlDatabase l_db = openReadOnly(l_path);
    QSqlQuery l_query(l_db);
    l_query.exec(QStringLiteral("SELECT identity_id FROM events"));
    QVERIFY(l_query.first());
    QVERIFY(l_query.value(0).toInt() > 0);

    l_query.exec(QStringLiteral("SELECT ipid, hwid FROM identities"));
    QVERIFY(l_query.first());
    QCOMPARE(l_query.value(0).toString(), QStringLiteral("abcd1234"));
    QCOMPARE(l_query.value(1).toString(), QString());
    closeReadOnly(l_db);
}

void tst_WriterSql::ipidAndHwidDenormalizedOnConnections()
{
    QTemporaryDir l_dir;
    QString l_path = l_dir.path() + "/events.db";

    {
        akashi::WriterSql l_writer(l_path);
        akashi::LogEvent l_event;
        l_event.timestamp = 1000;
        l_event.type = akashi::log_type::Connect;
        l_event.ipid = QStringLiteral("abcd1234");
        l_event.hwid = QStringLiteral("hw-001");
        l_writer.write(l_event);
        l_writer.flush();
    }

    QSqlDatabase l_db = openReadOnly(l_path);
    QSqlQuery l_query(l_db);
    l_query.exec(QStringLiteral("SELECT ipid, hwid, identity_id FROM connections"));
    l_query.first();
    QCOMPARE(l_query.value(0).toString(), QStringLiteral("abcd1234"));
    QCOMPARE(l_query.value(1).toString(), QStringLiteral("hw-001"));
    QVERIFY(l_query.value(2).toInt() > 0);
    closeReadOnly(l_db);
}

void tst_WriterSql::emptyFieldsStoreAsNull()
{
    QTemporaryDir l_dir;
    QString l_path = l_dir.path() + "/events.db";

    {
        akashi::WriterSql l_writer(l_path);
        akashi::LogEvent l_event;
        l_event.timestamp = 1000;
        l_event.type = akashi::log_type::IC;
        l_writer.write(l_event);
        l_writer.flush();
    }

    QSqlDatabase l_db = openReadOnly(l_path);
    QSqlQuery l_query(l_db);
    l_query.exec(QStringLiteral("SELECT area, ipid, hwid, identity_id, char_name, moderator FROM events"));
    l_query.first();
    QVERIFY(l_query.value(0).isNull());
    QVERIFY(l_query.value(1).isNull());
    QVERIFY(l_query.value(2).isNull());
    QVERIFY(l_query.value(3).isNull());
    QVERIFY(l_query.value(4).isNull());
    QVERIFY(l_query.value(5).isNull());
    closeReadOnly(l_db);
}

void tst_WriterSql::successFieldStoredCorrectly()
{
    QTemporaryDir l_dir;
    QString l_path = l_dir.path() + "/events.db";

    {
        akashi::WriterSql l_writer(l_path);

        akashi::LogEvent l_event;
        l_event.timestamp = 1000;
        l_event.type = akashi::log_type::CMD;
        l_event.success = true;
        l_writer.write(l_event);

        l_event.timestamp = 2000;
        l_event.success = false;
        l_writer.write(l_event);
        l_writer.flush();
    }

    QSqlDatabase l_db = openReadOnly(l_path);
    QSqlQuery l_query(l_db);
    l_query.exec(QStringLiteral("SELECT success FROM events ORDER BY timestamp"));
    l_query.first();
    QCOMPARE(l_query.value(0).toInt(), 1);
    l_query.next();
    QCOMPARE(l_query.value(0).toInt(), 0);
    closeReadOnly(l_db);
}

void tst_WriterSql::schemaIsWalMode()
{
    QTemporaryDir l_dir;
    QString l_path = l_dir.path() + "/events.db";

    {
        akashi::WriterSql l_writer(l_path);
        akashi::LogEvent l_event;
        l_event.timestamp = 1000;
        l_event.type = akashi::log_type::IC;
        l_writer.write(l_event);
        l_writer.flush();
    }

    QSqlDatabase l_db = openReadOnly(l_path);
    QSqlQuery l_query(l_db);
    l_query.exec(QStringLiteral("PRAGMA journal_mode"));
    l_query.first();
    QCOMPARE(l_query.value(0).toString(), QStringLiteral("wal"));
    closeReadOnly(l_db);
}

void tst_WriterSql::multipleFlushesAreIdempotent()
{
    QTemporaryDir l_dir;
    QString l_path = l_dir.path() + "/events.db";

    akashi::WriterSql l_writer(l_path);

    akashi::LogEvent l_event;
    l_event.timestamp = 1000;
    l_event.type = akashi::log_type::IC;
    l_writer.write(l_event);
    l_writer.flush();
    l_writer.flush();
    l_writer.flush();

    QSqlDatabase l_db = openReadOnly(l_path);
    QSqlQuery l_query(l_db);
    l_query.exec(QStringLiteral("SELECT COUNT(*) FROM events"));
    l_query.first();
    QCOMPARE(l_query.value(0).toInt(), 1);
    closeReadOnly(l_db);
}

void tst_WriterSql::reopenPreservesData()
{
    QTemporaryDir l_dir;
    QString l_path = l_dir.path() + "/events.db";

    {
        akashi::WriterSql l_writer(l_path);
        akashi::LogEvent l_event;
        l_event.timestamp = 1000;
        l_event.type = akashi::log_type::Connect;
        l_event.ipid = QStringLiteral("abc");
        l_event.hwid = QStringLiteral("hw");
        l_writer.write(l_event);
        l_writer.flush();
    }

    {
        akashi::WriterSql l_writer(l_path);
        akashi::LogEvent l_event;
        l_event.timestamp = 2000;
        l_event.type = akashi::log_type::IC;
        l_event.ipid = QStringLiteral("abc");
        l_event.hwid = QStringLiteral("hw");
        l_event.message = QStringLiteral("Hello");
        l_writer.write(l_event);
        l_writer.flush();
    }

    QSqlDatabase l_db = openReadOnly(l_path);
    QSqlQuery l_query(l_db);

    l_query.exec(QStringLiteral("SELECT COUNT(*) FROM connections"));
    l_query.first();
    QCOMPARE(l_query.value(0).toInt(), 1);

    l_query.exec(QStringLiteral("SELECT COUNT(*) FROM events"));
    l_query.first();
    QCOMPARE(l_query.value(0).toInt(), 1);

    l_query.exec(QStringLiteral("SELECT COUNT(*) FROM identities"));
    l_query.first();
    QCOMPARE(l_query.value(0).toInt(), 1);

    closeReadOnly(l_db);
}

void tst_WriterSql::ipidIndexExists()
{
    QTemporaryDir l_dir;
    QString l_path = l_dir.path() + "/events.db";

    {
        akashi::WriterSql l_writer(l_path);
        akashi::LogEvent l_event;
        l_event.timestamp = 1000;
        l_event.type = akashi::log_type::IC;
        l_writer.write(l_event);
        l_writer.flush();
    }

    QSqlDatabase l_db = openReadOnly(l_path);
    QSqlQuery l_query(l_db);

    const QStringList l_expected = {
        QStringLiteral("idx_connections_identity"),
        QStringLiteral("idx_connections_ipid"),
        QStringLiteral("idx_connections_hwid"),
        QStringLiteral("idx_events_identity"),
        QStringLiteral("idx_events_ipid"),
        QStringLiteral("idx_events_hwid"),
        QStringLiteral("idx_events_area"),
        QStringLiteral("idx_events_type"),
        QStringLiteral("idx_events_moderator"),
        QStringLiteral("idx_events_target"),
    };

    for (const QString &l_name : l_expected) {
        l_query.exec(QStringLiteral("SELECT name FROM sqlite_master WHERE type='index' AND name='%1'").arg(l_name));
        QVERIFY2(l_query.first(), qPrintable(l_name + " missing"));
    }

    closeReadOnly(l_db);
}

void tst_WriterSql::failedInsertWarnsInsteadOfDroppingSilently()
{
    QTemporaryDir l_dir;
    QString l_path = l_dir.path() + "/events.db";

    akashi::WriterSql l_writer(l_path);
    akashi::LogEvent l_event;
    l_event.timestamp = 1000;
    l_event.type = akashi::log_type::IC;
    l_writer.write(l_event);
    l_writer.flush();

    // Breaking the schema out from under the writer makes the inserts fail.
    {
        QSqlDatabase l_db = openReadOnly(l_path);
        QSqlQuery l_query(l_db);
        QVERIFY(l_query.exec(QStringLiteral("DROP TABLE events")));
        QVERIFY(l_query.exec(QStringLiteral("DROP TABLE connections")));
        closeReadOnly(l_db);
    }

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("WriterSql: event row dropped")));
    l_writer.write(l_event);

    akashi::LogEvent l_connect;
    l_connect.timestamp = 2000;
    l_connect.type = akashi::log_type::Connect;
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("WriterSql: connection row dropped")));
    l_writer.write(l_connect);
    l_writer.flush();
}

} // namespace unittests
} // namespace tests

QTEST_GUILESS_MAIN(tests::unittests::tst_WriterSql)

#include "tst_writer_sql.moc"
