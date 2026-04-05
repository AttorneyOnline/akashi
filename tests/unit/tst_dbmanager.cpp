// AI-generated: written by Claude.
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTest>

#include "akashi/database_service.h"
#include "db_manager.h"

namespace tests {
namespace unittests {

class tst_DBManager : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void olderActiveBanStillCounts();
    void lookupsAreIndexed();
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

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_DBManager)

#include "tst_dbmanager.moc"
