// AI-generated: written by Claude.
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

#include "akashi/database_service.h"

namespace tests {
namespace unittests {

using namespace akashi;

class tst_DatabaseService : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void nextOccurrenceIsAlwaysAhead();
    void maintenanceKeepsTheDatabaseUsable();
};

void tst_DatabaseService::nextOccurrenceIsAlwaysAhead()
{
    const QDateTime l_noon(QDate(2026, 7, 5), QTime(12, 0));

    // Later the same day: four and a half hours ahead.
    QCOMPARE(DatabaseService::msecsToNextOccurrence(QTime(16, 30), l_noon), qint64(16.5 * 60 * 60 * 1000) - qint64(12 * 60 * 60 * 1000));

    // Earlier in the day means tomorrow.
    QCOMPARE(DatabaseService::msecsToNextOccurrence(QTime(4, 0), l_noon), qint64(16 * 60 * 60 * 1000));

    // Exactly now also means tomorrow, never zero.
    QCOMPARE(DatabaseService::msecsToNextOccurrence(QTime(12, 0), l_noon), qint64(24 * 60 * 60 * 1000));
}

void tst_DatabaseService::maintenanceKeepsTheDatabaseUsable()
{
    QTemporaryDir l_dir;
    QVERIFY(l_dir.isValid());
    DatabaseService l_service(l_dir.path());
    QVERIFY(l_service.open());

    QSqlQuery l_query(l_service.database());
    QVERIFY(l_query.exec("CREATE TABLE things('NAME' TEXT)"));
    QVERIFY(l_query.exec("INSERT INTO things VALUES('before')"));

    // An invalid time schedules nothing, but maintenance can still run by hand.
    l_service.scheduleMaintenance(QTime(), true);
    l_service.runMaintenance();

    QVERIFY(l_query.exec("INSERT INTO things VALUES('after')"));
    QVERIFY(l_query.exec("SELECT COUNT(*) FROM things"));
    QVERIFY(l_query.first());
    QCOMPARE(l_query.value(0).toInt(), 2);
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_DatabaseService)

#include "tst_databaseservice.moc"
