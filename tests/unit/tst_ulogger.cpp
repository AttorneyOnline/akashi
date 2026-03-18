// AI-generated: written by Claude.
#include <QTest>

#include "akashi/config_store.h"
#include "config_manager.h"
#include "u_logger.h"

namespace tests {
namespace unittests {

class tst_ULogger : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase();
    void kickEntryContainsReason();
    void banEntryContainsReason();
    void adduserEntryUsesTemplate();
};

void tst_ULogger::initTestCase()
{
    ConfigManager::setStore(new akashi::ConfigStore("config", this));
}

void tst_ULogger::kickEntryContainsReason()
{
    ULogger logger;
    logger.logKick("Moderator", "1234", "spamming");
    QVERIFY(logger.buffer("SERVER").last().contains("spamming"));
}

void tst_ULogger::banEntryContainsReason()
{
    ULogger logger;
    logger.logBan("Moderator", "1234", "5 hours", "spamming");
    QVERIFY(logger.buffer("SERVER").last().contains("spamming"));
}

void tst_ULogger::adduserEntryUsesTemplate()
{
    ULogger logger;
    logger.logCMD("Phoenix", "1234", "Nick", "adduser", {"newmod"}, "Basement");
    QVERIFY(logger.buffer("Basement").last().contains("USERADD"));
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_ULogger)

#include "tst_ulogger.moc"
