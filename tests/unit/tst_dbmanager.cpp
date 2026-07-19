// AI-generated: written by Claude.
#include "akashi/database_service.h"
#include "core/db_manager.h"

#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTest>

namespace tests {
namespace unittests {

class tst_DBManager : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void olderActiveBanStillCounts();
    void banChecksFailClosedWhenTheQueryFails();
    void userWritesFailClosedWhenTheQueryFails();
    void lookupsAreIndexed();
    void sanctionsStoreAndExpire();
    void untimedSanctionsHoldUntilLifted();
    void sanctionsMatchByHwid();
    void oldSanctionTablesGainTheNewColumns();
};

void tst_DBManager::olderActiveBanStillCounts()
{
    QSqlDatabase l_database = QSqlDatabase::addDatabase("QSQLITE", "test_db");
    l_database.setDatabaseName(":memory:");
    QVERIFY(l_database.open());
    DBManager l_manager(l_database);

    const unsigned long l_now = QDateTime::currentDateTime().toSecsSinceEpoch();

    // An old permanent ban, followed by a newer ban that already expired.
    DBManager::BanInfo l_permanent;
    l_permanent.ipid = "1234";
    l_permanent.time = l_now - 1000;
    l_permanent.duration = -2;
    l_manager.addBan(l_permanent);

    DBManager::BanInfo l_expired;
    l_expired.ipid = "1234";
    l_expired.time = l_now - 100;
    l_expired.duration = 10;
    l_manager.addBan(l_expired);

    // The permanent ban must still apply even though the newest ban expired.
    const auto l_result = l_manager.isIPBanned("1234");
    QCOMPARE(l_result.first, true);
    QCOMPARE(l_result.second.duration, -2);
}

// A ban lookup whose query fails must read as banned, not as "no ban found" -
// otherwise a database outage silently readmits every banned player.
void tst_DBManager::banChecksFailClosedWhenTheQueryFails()
{
    QSqlDatabase l_database = QSqlDatabase::addDatabase("QSQLITE", "test_db_fail_closed");
    l_database.setDatabaseName(":memory:");
    QVERIFY(l_database.open());
    DBManager l_manager(l_database);

    // Breaking the schema out from under the manager makes every ban SELECT fail.
    QSqlQuery l_break(l_database);
    QVERIFY(l_break.exec("DROP TABLE bans"));

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("Ban check failed for ipid")));
    const auto l_ip_result = l_manager.isIPBanned("1234");
    QCOMPARE(l_ip_result.first, true);
    QVERIFY(l_ip_result.second.reason.contains(QStringLiteral("could not be checked")));

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("Ban check failed for hdid")));
    const auto l_hdid_result = l_manager.isHDIDBanned("abcd");
    QCOMPARE(l_hdid_result.first, true);
    QVERIFY(l_hdid_result.second.reason.contains(QStringLiteral("could not be checked")));
}

void tst_DBManager::userWritesFailClosedWhenTheQueryFails()
{
    QSqlDatabase l_database = QSqlDatabase::addDatabase("QSQLITE", "test_db_user_fail_closed");
    l_database.setDatabaseName(":memory:");
    QVERIFY(l_database.open());
    DBManager l_manager(l_database);

    // Breaking the schema out from under the manager makes every user query fail.
    QSqlQuery l_break(l_database);
    QVERIFY(l_break.exec("DROP TABLE users"));

    // A failed existence check must report failure, not fall through to the
    // insert and claim success.
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("user existence check failed")));
    QVERIFY(!l_manager.createUser("mod", QByteArray(16, 'a'), "hunter2!A", "MOD"));

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("password update failed")));
    QVERIFY(!l_manager.updatePassword("mod", "hunter3!A"));

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("user existence check failed")));
    QVERIFY(!l_manager.updateACL("mod", "MOD"));

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("user existence check failed")));
    QVERIFY(!l_manager.deleteUser("mod"));
}

void tst_DBManager::lookupsAreIndexed()
{
    QSqlDatabase l_database = QSqlDatabase::addDatabase("QSQLITE", "test_db_indexes");
    l_database.setDatabaseName(":memory:");
    QVERIFY(l_database.open());
    DBManager l_manager(l_database);

    // The migrations must end at the current version with every lookup index in place.
    QCOMPARE(akashi::DatabaseService::schemaVersion(l_database), DB_VERSION);

    QStringList l_indexes;
    QSqlQuery l_query(l_database);
    l_query.exec("SELECT name FROM sqlite_master WHERE type = 'index'");
    while (l_query.next()) {
        l_indexes.append(l_query.value(0).toString());
    }
    QVERIFY(l_indexes.contains("bans_ipid_time"));
    QVERIFY(l_indexes.contains("bans_hdid_time"));
    QVERIFY(l_indexes.contains("bans_ip"));
    QVERIFY(l_indexes.contains("users_username"));
}

void tst_DBManager::sanctionsStoreAndExpire()
{
    QSqlDatabase l_database = QSqlDatabase::addDatabase("QSQLITE", "test_db_sanctions");
    l_database.setDatabaseName(":memory:");
    QVERIFY(l_database.open());
    DBManager l_manager(l_database);

    const qint64 l_now = QDateTime::currentSecsSinceEpoch();
    l_manager.upsertSanction({"1234", "muted", "moderator", l_now, l_now + 600});
    l_manager.upsertSanction({"1234", "gimped", "moderator", l_now, l_now - 5});
    l_manager.upsertSanction({"5678", "muted", "moderator", l_now, l_now + 600});

    // Only the still-active sanctions of the asked-for IPID come back.
    QList<DBManager::SanctionInfo> l_active = l_manager.sanctionsFor("1234", l_now);
    QCOMPARE(l_active.size(), 1);
    QCOMPARE(l_active.first().sanction, QStringLiteral("muted"));
    QCOMPARE(l_active.first().expires, l_now + 600);

    // Storing the same pair again replaces instead of stacking.
    l_manager.upsertSanction({"1234", "muted", "another moderator", l_now, l_now + 1200});
    l_active = l_manager.sanctionsFor("1234", l_now);
    QCOMPARE(l_active.size(), 1);
    QCOMPARE(l_active.first().expires, l_now + 1200);
    QCOMPARE(l_active.first().moderator, QStringLiteral("another moderator"));

    // The boot pass sees everything, the expired row included.
    QCOMPARE(l_manager.allSanctions().size(), 3);

    l_manager.removeSanction("1234", "muted");
    QCOMPARE(l_manager.sanctionsFor("1234", l_now).size(), 0);
    QCOMPARE(l_manager.allSanctions().size(), 2);
}

void tst_DBManager::untimedSanctionsHoldUntilLifted()
{
    QSqlDatabase l_database = QSqlDatabase::addDatabase("QSQLITE", "test_db_untimed");
    l_database.setDatabaseName(":memory:");
    QVERIFY(l_database.open());
    DBManager l_manager(l_database);

    const qint64 l_now = QDateTime::currentSecsSinceEpoch();

    // An untimed sanction stores -1 and stays in force at any moment,
    // however far in the future the check runs.
    l_manager.upsertSanction({"1234", "muted", "moderator", l_now, -1});
    QCOMPARE(l_manager.sanctionsFor("1234", l_now).size(), 1);
    QCOMPARE(l_manager.sanctionsFor("1234", l_now + 1000000).size(), 1);

    // The payload column survives the round trip - the charcurse list.
    l_manager.upsertSanction({"1234", "charcurse", "moderator", l_now, -1, "", "Phoenix,Edgeworth"});
    const auto l_row = l_manager.sanctionRow("1234", "charcurse");
    QVERIFY(l_row.has_value());
    QCOMPARE(l_row->data, QStringLiteral("Phoenix,Edgeworth"));
    QCOMPARE(l_row->expires, qint64(-1));
    QVERIFY(!l_manager.sanctionRow("1234", "gimped").has_value());

    // A later timed apply replaces the untimed row, and lifting clears it.
    l_manager.upsertSanction({"1234", "muted", "moderator", l_now, l_now + 600});
    QCOMPARE(l_manager.sanctionRow("1234", "muted")->expires, l_now + 600);
    l_manager.removeSanction("1234", "muted");
    QVERIFY(!l_manager.sanctionRow("1234", "muted").has_value());
}

void tst_DBManager::sanctionsMatchByHwid()
{
    QSqlDatabase l_database = QSqlDatabase::addDatabase("QSQLITE", "test_db_hwid");
    l_database.setDatabaseName(":memory:");
    QVERIFY(l_database.open());
    DBManager l_manager(l_database);

    const qint64 l_now = QDateTime::currentSecsSinceEpoch();
    l_manager.upsertSanction({"1234", "muted", "moderator", l_now, -1, "HW-A"});
    l_manager.upsertSanction({"5678", "gimped", "moderator", l_now, -1, "HW-B"});

    // A new IP (fresh IPID) with the sanctioned hardware still matches.
    QList<DBManager::SanctionInfo> l_matched = l_manager.sanctionsForIdentity("9999", "HW-A", l_now);
    QCOMPARE(l_matched.size(), 1);
    QCOMPARE(l_matched.first().sanction, QStringLiteral("muted"));

    // Matching by both identifiers returns the row once, not twice.
    QCOMPARE(l_manager.sanctionsForIdentity("1234", "HW-A", l_now).size(), 1);

    // No hardware id known yet falls back to the IPID check, and an
    // empty stored HWID never matches anybody by hardware.
    QCOMPARE(l_manager.sanctionsForIdentity("1234", "", l_now).size(), 1);
    l_manager.upsertSanction({"aaaa", "shaken", "moderator", l_now, -1, ""});
    QCOMPARE(l_manager.sanctionsForIdentity("bbbb", "HW-C", l_now).size(), 0);
}

void tst_DBManager::oldSanctionTablesGainTheNewColumns()
{
    QSqlDatabase l_database = QSqlDatabase::addDatabase("QSQLITE", "test_db_sanction_migration");
    l_database.setDatabaseName(":memory:");
    QVERIFY(l_database.open());

    // A version-4 database with the old five-column sanctions table and a
    // row already stored.
    QSqlQuery l_setup(l_database);
    QVERIFY(l_setup.exec("CREATE TABLE bans ('ID' INTEGER, 'IPID' TEXT, 'HDID' TEXT, 'IP' TEXT, 'TIME' INTEGER, 'REASON' TEXT, 'DURATION' INTEGER, 'MODERATOR' TEXT, PRIMARY KEY('ID' AUTOINCREMENT))"));
    QVERIFY(l_setup.exec("CREATE TABLE users ('ID' INTEGER, 'USERNAME' TEXT, 'SALT' TEXT, 'PASSWORD' TEXT, 'ACL' TEXT, PRIMARY KEY('ID' AUTOINCREMENT))"));
    QVERIFY(l_setup.exec("CREATE TABLE sanctions ('ID' INTEGER, 'IPID' TEXT NOT NULL, 'SANCTION' TEXT NOT NULL, 'MODERATOR' TEXT, 'ISSUED' INTEGER, 'EXPIRES' INTEGER NOT NULL, PRIMARY KEY('ID' AUTOINCREMENT), UNIQUE('IPID', 'SANCTION'))"));
    const qint64 l_now = QDateTime::currentSecsSinceEpoch();
    QVERIFY(l_setup.exec(QStringLiteral("INSERT INTO sanctions(IPID, SANCTION, MODERATOR, ISSUED, EXPIRES) VALUES('1234', 'muted', 'moderator', %1, %2)").arg(l_now).arg(l_now + 600)));
    QVERIFY(l_setup.exec("PRAGMA user_version = 4"));

    // Opening it migrates to the current version; the old row reads back
    // with empty new columns and the new shape stores fine.
    DBManager l_manager(l_database);
    QCOMPARE(akashi::DatabaseService::schemaVersion(l_database), DB_VERSION);
    const auto l_old_row = l_manager.sanctionRow("1234", "muted");
    QVERIFY(l_old_row.has_value());
    QCOMPARE(l_old_row->hwid, QString());
    QCOMPARE(l_old_row->data, QString());
    l_manager.upsertSanction({"5678", "charcurse", "moderator", l_now, -1, "HW-A", "Phoenix"});
    QCOMPARE(l_manager.sanctionsForIdentity("0000", "HW-A", l_now).size(), 1);
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_DBManager)

#include "tst_dbmanager.moc"
