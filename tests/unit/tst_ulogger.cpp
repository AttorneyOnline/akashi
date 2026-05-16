// AI-generated: written by Claude.
#include "u_logger.h"

#include <QTest>

namespace tests {
namespace unittests {

class tst_ULogger : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void initTestCase();
    void kickEntryContainsReason();
    void banEntryContainsReason();
    void cmdEntryContainsArgs();
    void redactedArgsShowStars();
};

void tst_ULogger::initTestCase()
{
}

void tst_ULogger::kickEntryContainsReason()
{
    ULogger logger(nullptr);
    logger.logKick("Moderator", "1234", "spamming");
    QVERIFY(logger.buffer("SERVER").last().contains("spamming"));
}

void tst_ULogger::banEntryContainsReason()
{
    ULogger logger(nullptr);
    logger.logBan("Moderator", "1234", "5 hours", "spamming");
    QVERIFY(logger.buffer("SERVER").last().contains("spamming"));
}

void tst_ULogger::cmdEntryContainsArgs()
{
    ULogger logger(nullptr);
    logger.logCMD("Phoenix", "1234", "Nick", "adduser", {"newmod", "***"}, "Basement");
    QString l_entry = logger.buffer("Basement").last();
    QVERIFY(l_entry.contains("adduser"));
    QVERIFY(l_entry.contains("newmod"));
}

void tst_ULogger::redactedArgsShowStars()
{
    ULogger logger(nullptr);
    logger.logCMD("Phoenix", "1234", "Nick", "changepass", {"***"}, "Basement");
    QString l_entry = logger.buffer("Basement").last();
    QVERIFY(l_entry.contains("changepass"));
    QVERIFY(l_entry.contains("***"));
    QVERIFY(!l_entry.contains("secret"));
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_ULogger)

#include "tst_ulogger.moc"
