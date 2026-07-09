// AI-generated: written by Claude.
#include "akashi/database_service.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

namespace tests {
namespace unittests {

using namespace akashi;

class tst_DatabaseService : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void maintenanceKeepsTheDatabaseUsable();
    void backupsWriteAndPrune();
    void readerRefusesWrites();
    void readerOfMissingSourceIsInvalid();
    void failedMigrationRollsBackCompletely();
};

void tst_DatabaseService::maintenanceKeepsTheDatabaseUsable()
{
    QTemporaryDir l_dir;
    QVERIFY(l_dir.isValid());
    DatabaseService l_service(l_dir.path());
    QVERIFY(l_service.open());

    QSqlQuery l_query(l_service.database());
    QVERIFY(l_query.exec("CREATE TABLE things('NAME' TEXT)"));
    QVERIFY(l_query.exec("INSERT INTO things VALUES('before')"));

    l_service.runMaintenance(true);

    QVERIFY(l_query.exec("INSERT INTO things VALUES('after')"));
    QVERIFY(l_query.exec("SELECT COUNT(*) FROM things"));
    QVERIFY(l_query.first());
    QCOMPARE(l_query.value(0).toInt(), 2);
}

void tst_DatabaseService::backupsWriteAndPrune()
{
    QTemporaryDir l_dir;
    QVERIFY(l_dir.isValid());
    DatabaseService l_service(l_dir.path());
    QVERIFY(l_service.open());

    QSqlQuery l_query(l_service.database());
    QVERIFY(l_query.exec("CREATE TABLE souvenirs('NOTE' TEXT)"));
    QVERIFY(l_query.exec("INSERT INTO souvenirs VALUES('backed up')"));

    // Two stale copies from years past, so retention has something to prune.
    const QString l_backup_root = l_dir.path() + "/backups";
    QVERIFY(QDir().mkpath(l_backup_root));
    for (const QString &l_stale : {QStringLiteral("akashi-20200101-000000.db"), QStringLiteral("akashi-20200102-000000.db")}) {
        QFile l_file(l_backup_root + "/" + l_stale);
        QVERIFY(l_file.open(QIODevice::WriteOnly));
        l_file.write("stale");
    }

    QCOMPARE(l_service.runBackups(2), 1);

    // The oldest stale copy is pruned; the fresh backup is a working database.
    const QStringList l_backups = QDir(l_backup_root).entryList({"akashi-*.db"}, QDir::Files, QDir::Name);
    QCOMPARE(l_backups.size(), 2);
    QVERIFY(!l_backups.contains(QStringLiteral("akashi-20200101-000000.db")));

    QSqlDatabase l_copy = QSqlDatabase::addDatabase("QSQLITE", "backup_check");
    l_copy.setDatabaseName(l_backup_root + "/" + l_backups.last());
    QVERIFY(l_copy.open());
    QSqlQuery l_verify(l_copy);
    QVERIFY(l_verify.exec("SELECT NOTE FROM souvenirs"));
    QVERIFY(l_verify.first());
    QCOMPARE(l_verify.value(0).toString(), QStringLiteral("backed up"));
    l_copy.close();
    QSqlDatabase::removeDatabase("backup_check");
}

void tst_DatabaseService::readerRefusesWrites()
{
    QTemporaryDir l_dir;
    QVERIFY(l_dir.isValid());
    DatabaseService l_service(l_dir.path());
    QVERIFY(l_service.open());

    QSqlQuery l_query(l_service.database());
    QVERIFY(l_query.exec("CREATE TABLE vault('GOLD' TEXT)"));
    QVERIFY(l_query.exec("INSERT INTO vault VALUES('safe')"));

    QSqlDatabase l_reader = l_service.reader(QStringLiteral("main"));
    QVERIFY(l_reader.isOpen());

    // Reading works; the engine itself refuses every write.
    QSqlQuery l_read(l_reader);
    QVERIFY(l_read.exec("SELECT GOLD FROM vault"));
    QVERIFY(l_read.first());
    QCOMPARE(l_read.value(0).toString(), QStringLiteral("safe"));

    QSqlQuery l_write(l_reader);
    QVERIFY(!l_write.exec("INSERT INTO vault VALUES('stolen')"));
    QVERIFY(!l_write.exec("DROP TABLE vault"));

    QVERIFY(l_query.exec("SELECT COUNT(*) FROM vault"));
    QVERIFY(l_query.first());
    QCOMPARE(l_query.value(0).toInt(), 1);
}

void tst_DatabaseService::readerOfMissingSourceIsInvalid()
{
    QTemporaryDir l_dir;
    QVERIFY(l_dir.isValid());
    DatabaseService l_service(l_dir.path());
    QVERIFY(l_service.open());

    // A plugin that never made a database has no file to read.
    QSqlDatabase l_reader = l_service.reader(QStringLiteral("no.such.plugin"));
    QVERIFY(!l_reader.isValid());
    QVERIFY(!l_reader.isOpen());
}

void tst_DatabaseService::failedMigrationRollsBackCompletely()
{
    QTemporaryDir l_dir;
    QVERIFY(l_dir.isValid());
    DatabaseService l_service(l_dir.path());
    QVERIFY(l_service.open());

    QSqlQuery l_query(l_service.database());
    QVERIFY(l_query.exec("CREATE TABLE ledger('ENTRY' TEXT)"));

    const int l_before = DatabaseService::schemaVersion(l_service.database());

    // The migration writes a row and then reports failure; the transaction
    // takes the row with it and the version does not move.
    QTest::ignoreMessage(QtCriticalMsg, QRegularExpression("migration to version"));
    QVERIFY(!DatabaseService::applyMigration(l_service.database(), l_before + 1, [](QSqlDatabase &f_database) {
        QSqlQuery l_write(f_database);
        l_write.exec("INSERT INTO ledger VALUES('phantom')");
        return false;
    }));

    QCOMPARE(DatabaseService::schemaVersion(l_service.database()), l_before);
    QVERIFY(l_query.exec("SELECT COUNT(*) FROM ledger"));
    QVERIFY(l_query.first());
    QCOMPARE(l_query.value(0).toInt(), 0);
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_DatabaseService)

#include "tst_databaseservice.moc"
