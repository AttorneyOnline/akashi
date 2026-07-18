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

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_DBManager)

#include "tst_dbmanager.moc"
