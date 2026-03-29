// AI-generated: written by Claude.
#include <QSqlDatabase>
#include <QTest>

#include "db_manager.h"

namespace tests {
namespace unittests {

class tst_DBManager : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void olderActiveBanStillCounts();
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

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_DBManager)

#include "tst_dbmanager.moc"
