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

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_FileSystem)

#include "tst_filesystem.moc"
