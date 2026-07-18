// AI-generated: written by Claude.
#include "core/exit_code.h"
#include "core/server_context.h"

#include <QRegularExpression>
#include <QTest>

namespace tests {
namespace unittests {

class tst_ServerLifecycle : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void failedStartDestructsCleanly();
};

// A start that fails before the core is built leaves a half-initialized
// context; its destructor used to delete uninitialized pointers and take
// the process down with an access violation instead of the exit code
// naming the configuration problem.
void tst_ServerLifecycle::failedStartDestructsCleanly()
{
    qputenv("AKASHI_CONFIG_ROOT", "does-not-exist");
    {
        QTest::ignoreMessage(QtCriticalMsg, QRegularExpression(QStringLiteral("does not exist")));
        QTest::ignoreMessage(QtCriticalMsg, QRegularExpression(QStringLiteral("configuration is invalid")));
        ServerContext l_context;
        QVERIFY(l_context.start() == ExitCode::InvalidConfig);
        // Shutting down twice is the documented contract; the second run
        // (the destructor's safety net) must stay a no-op.
        l_context.shutdown();
    }
    qunsetenv("AKASHI_CONFIG_ROOT");
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_ServerLifecycle)

#include "tst_server_lifecycle.moc"
