// AI-generated: written by Claude.
#include <QTest>

#include "akashi/filesystem_service.h"

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
    // A user path may not climb out of storage into the rest of the application.
    QVERIFY(!l_fs.resolve(Scope::Storage, "../config/config.json").has_value());
    QVERIFY(!l_fs.resolve(Scope::Storage, "../../etc/passwd").has_value());
}

void tst_FileSystem::systemReachesTheWholeApplication()
{
    FileSystemService l_fs("/app");
    // System scope may reach any application folder, including storage.
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
    // Even system scope may not climb above the application root.
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
    // A .. that stays within the boundary is allowed.
    const auto l_result = l_fs.resolve(Scope::Storage, "a/../b.txt");
    QVERIFY(l_result.has_value());
    QCOMPARE(*l_result, QString("/app/storage/b.txt"));
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_FileSystem)

#include "tst_filesystem.moc"
