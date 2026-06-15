// AI-generated: written by Claude.
#include "akashi/filesystem_service.h"

#include <QTest>
#include <QTemporaryDir>

namespace tests {
namespace unittests {

using namespace akashi;
using Scope = FileSystemService::Scope;

class tst_FileSystem : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void storageResolvesInsideStorage();
    void storageRejectsEscape();
    void systemReachesTheWholeApplication();
    void systemRejectsEscapeAboveRoot();
    void absolutePathsAreRejected();
    void lexicalParentInsideBoundaryIsFine();
    void serviceIdAndVersion();
    void commonFolderAccessors();
    void pluginDataDirCreatesDirectory();
    void pluginResolveConfinesPaths();
    void pluginResolveRejectsEscape();
    void spaceCheckHoldsTheMargin();
    void writeFileWritesAtomicallyAndReadsBack();
    void sanitizedFileName_data();
    void sanitizedFileName();
};

void tst_FileSystem::storageResolvesInsideStorage()
{
    FileSystemService l_fs("/app");
    const auto l_result = l_fs.resolve(Scope::Storage, "testimony/case1.txt");
    QVERIFY(l_result.has_value());
    QCOMPARE(*l_result, QString("/app/storage/testimony/case1.txt"));
}

void tst_FileSystem::storageRejectsEscape()
{
    FileSystemService l_fs("/app");
    QVERIFY(!l_fs.resolve(Scope::Storage, "../config/config.json").has_value());
    QVERIFY(!l_fs.resolve(Scope::Storage, "../../etc/passwd").has_value());
}

void tst_FileSystem::systemReachesTheWholeApplication()
{
    FileSystemService l_fs("/app");
    const auto l_config = l_fs.resolve(Scope::System, "config/config.json");
    QVERIFY(l_config.has_value());
    QCOMPARE(*l_config, QString("/app/config/config.json"));
    const auto l_storage = l_fs.resolve(Scope::System, "storage/testimony/case1.txt");
    QVERIFY(l_storage.has_value());
    QCOMPARE(*l_storage, QString("/app/storage/testimony/case1.txt"));
}

void tst_FileSystem::systemRejectsEscapeAboveRoot()
{
    FileSystemService l_fs("/app");
    QVERIFY(!l_fs.resolve(Scope::System, "../secrets").has_value());
    QVERIFY(!l_fs.resolve(Scope::System, "../../etc/passwd").has_value());
}

void tst_FileSystem::absolutePathsAreRejected()
{
    FileSystemService l_fs("/app");
    QVERIFY(!l_fs.resolve(Scope::Storage, "/etc/passwd").has_value());
}

void tst_FileSystem::lexicalParentInsideBoundaryIsFine()
{
    FileSystemService l_fs("/app");
    const auto l_result = l_fs.resolve(Scope::Storage, "a/../b.txt");
    QVERIFY(l_result.has_value());
    QCOMPARE(*l_result, QString("/app/storage/b.txt"));
}

void tst_FileSystem::serviceIdAndVersion()
{
    FileSystemService l_fs("/app");
    QCOMPARE(l_fs.serviceId(), QStringLiteral("akashi.filesystem"));
    QCOMPARE(l_fs.serviceVersion().major, 1);
}

void tst_FileSystem::commonFolderAccessors()
{
    FileSystemService l_fs("/app");
    QCOMPARE(l_fs.configRoot(), QString("/app/config"));
    QCOMPARE(l_fs.dataRoot(), QString("/app/data"));
    QCOMPARE(l_fs.storageRoot(), QString("/app/storage"));
    QCOMPARE(l_fs.pluginsRoot(), QString("/app/data/plugins"));
}

void tst_FileSystem::pluginDataDirCreatesDirectory()
{
    QTemporaryDir l_tmp;
    QVERIFY(l_tmp.isValid());
    FileSystemService l_fs(l_tmp.path());

    const QString l_dir = l_fs.pluginDataDir("my-plugin");
    QVERIFY(QDir(l_dir).exists());
    QVERIFY(l_dir.endsWith("/data/plugins/my-plugin"));
}

void tst_FileSystem::pluginResolveConfinesPaths()
{
    FileSystemService l_fs("/app");
    const auto l_result = l_fs.pluginResolve("my-plugin", "logs/events.db");
    QVERIFY(l_result.has_value());
    QCOMPARE(*l_result, QString("/app/data/plugins/my-plugin/logs/events.db"));
}

void tst_FileSystem::pluginResolveRejectsEscape()
{
    FileSystemService l_fs("/app");
    QVERIFY(!l_fs.pluginResolve("my-plugin", "../other-plugin/data.db").has_value());
    QVERIFY(!l_fs.pluginResolve("my-plugin", "../../config/secret.json").has_value());
    QVERIFY(!l_fs.pluginResolve("my-plugin", "/etc/passwd").has_value());
}

void tst_FileSystem::spaceCheckHoldsTheMargin()
{
    QTemporaryDir l_tmp;
    FileSystemService l_fs(l_tmp.path());

    // A small write fits; one the size of the volume itself never does.
    QVERIFY(l_fs.hasSpaceFor(l_tmp.filePath("small.txt"), 1024));
    QVERIFY(!l_fs.hasSpaceFor(l_tmp.filePath("giant.txt"), Q_INT64_C(9000000000000000000)));

    // A target whose folders do not exist yet still finds its volume.
    QVERIFY(l_fs.hasSpaceFor(l_tmp.filePath("not/yet/made/file.txt"), 1024));
}

void tst_FileSystem::writeFileWritesAtomicallyAndReadsBack()
{
    QTemporaryDir l_tmp;
    FileSystemService l_fs(l_tmp.path());

    const QString l_path = l_tmp.filePath("storage/notes/case.txt");
    QVERIFY(!l_fs.writeFile(l_path, "first draft\n").has_value());

    QFile l_file(l_path);
    QVERIFY(l_file.open(QIODevice::ReadOnly));
    QCOMPARE(l_file.readAll(), QByteArray("first draft\n"));
    l_file.close();

    // Overwriting replaces the whole file, never appends.
    QVERIFY(!l_fs.writeFile(l_path, "final\n").has_value());
    QVERIFY(l_file.open(QIODevice::ReadOnly));
    QCOMPARE(l_file.readAll(), QByteArray("final\n"));
}

void tst_FileSystem::sanitizedFileName_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<bool>("valid");
    QTest::addColumn<QString>("expected");

    QTest::addRow("Plain name") << "my_testimony-1" << true << "my_testimony-1";
    QTest::addRow("Lowercased and trimmed") << "  Case1  " << true << "case1";
    QTest::addRow("Parent traversal") << "../../etc/passwd" << false << "";
    QTest::addRow("Subdirectory") << "sub/name" << false << "";
    QTest::addRow("Backslash") << "sub\\name" << false << "";
    QTest::addRow("Dots") << "case.1" << false << "";
    QTest::addRow("Empty") << "" << false << "";
    QTest::addRow("Spaces inside") << "my case" << false << "";
}

void tst_FileSystem::sanitizedFileName()
{
    QFETCH(QString, input);
    QFETCH(bool, valid);
    QFETCH(QString, expected);

    const std::optional<QString> l_result = FileSystemService::sanitizedFileName(input);
    QCOMPARE(l_result.has_value(), valid);
    if (valid) {
        QCOMPARE(*l_result, expected);
    }
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_FileSystem)

#include "tst_filesystem.moc"
