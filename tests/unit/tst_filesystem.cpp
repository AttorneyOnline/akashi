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

  private slots:
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
    QCOMPARE(l_fs.resolve(Scope::Storage, "testimony/case1.txt"), "/app/storage/testimony/case1.txt");
}

void tst_FileSystem::storageRejectsEscape()
{
    FileSystemService l_fs("/app");
    // A user path may not climb out of storage into the rest of the application.
    QCOMPARE(l_fs.resolve(Scope::Storage, "../config/config.json"), QString());
    QCOMPARE(l_fs.resolve(Scope::Storage, "../../etc/passwd"), QString());
}

void tst_FileSystem::systemReachesTheWholeApplication()
{
    FileSystemService l_fs("/app");
    // System scope may reach any application folder, including storage.
    QCOMPARE(l_fs.resolve(Scope::System, "config/config.json"), "/app/config/config.json");
    QCOMPARE(l_fs.resolve(Scope::System, "storage/testimony/case1.txt"), "/app/storage/testimony/case1.txt");
}

void tst_FileSystem::systemRejectsEscapeAboveRoot()
{
    FileSystemService l_fs("/app");
    // Even system scope may not climb above the application root.
    QCOMPARE(l_fs.resolve(Scope::System, "../secrets"), QString());
    QCOMPARE(l_fs.resolve(Scope::System, "../../etc/passwd"), QString());
}

void tst_FileSystem::absolutePathsAreRejected()
{
    FileSystemService l_fs("/app");
    QCOMPARE(l_fs.resolve(Scope::Storage, "/etc/passwd"), QString());
}

void tst_FileSystem::lexicalParentInsideBoundaryIsFine()
{
    FileSystemService l_fs("/app");
    // A .. that stays within the boundary is allowed.
    QCOMPARE(l_fs.resolve(Scope::Storage, "a/../b.txt"), "/app/storage/b.txt");
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_FileSystem)

#include "tst_filesystem.moc"
