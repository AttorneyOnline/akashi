// AI-generated: written by Claude.
#include "core/auth_throttle.h"

#include <QTest>

namespace tests {
namespace unittests {

using namespace akashi;

class tst_AuthThrottle : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void freshIpidIsNotLockedOut();
    void belowLimitIsNotLockedOut();
    void atLimitLocksOut();
    void successResetsCount();
    void resetClearsRecord();
    void lockoutExpiresAfterTimeout();
    void remainingSecondsReportsCorrectly();
    void setLimitsChangesThreshold();
    void expiredLockoutResetsCountOnNextFailure();
    void separateIpidsAreIndependent();
};

void tst_AuthThrottle::freshIpidIsNotLockedOut()
{
    AuthThrottle l_throttle(3, 10);
    QVERIFY(!l_throttle.isLockedOut("abc123"));
    QCOMPARE(l_throttle.remainingLockoutSeconds("abc123"), 0);
}

void tst_AuthThrottle::belowLimitIsNotLockedOut()
{
    AuthThrottle l_throttle(3, 10);
    l_throttle.recordFailure("abc123");
    l_throttle.recordFailure("abc123");
    QVERIFY(!l_throttle.isLockedOut("abc123"));
}

void tst_AuthThrottle::atLimitLocksOut()
{
    AuthThrottle l_throttle(3, 10);
    l_throttle.recordFailure("abc123");
    l_throttle.recordFailure("abc123");
    l_throttle.recordFailure("abc123");
    QVERIFY(l_throttle.isLockedOut("abc123"));
}

void tst_AuthThrottle::successResetsCount()
{
    AuthThrottle l_throttle(3, 10);
    l_throttle.recordFailure("abc123");
    l_throttle.recordFailure("abc123");
    l_throttle.recordSuccess("abc123");
    QVERIFY(!l_throttle.isLockedOut("abc123"));

    l_throttle.recordFailure("abc123");
    l_throttle.recordFailure("abc123");
    QVERIFY(!l_throttle.isLockedOut("abc123"));
}

void tst_AuthThrottle::resetClearsRecord()
{
    AuthThrottle l_throttle(3, 10);
    l_throttle.recordFailure("abc123");
    l_throttle.recordFailure("abc123");
    l_throttle.recordFailure("abc123");
    QVERIFY(l_throttle.isLockedOut("abc123"));

    l_throttle.reset("abc123");
    QVERIFY(!l_throttle.isLockedOut("abc123"));
}

void tst_AuthThrottle::lockoutExpiresAfterTimeout()
{
    AuthThrottle l_throttle(2, 1);
    l_throttle.recordFailure("abc123");
    l_throttle.recordFailure("abc123");
    QVERIFY(l_throttle.isLockedOut("abc123"));

    QTest::qWait(1200);
    QVERIFY(!l_throttle.isLockedOut("abc123"));
}

void tst_AuthThrottle::remainingSecondsReportsCorrectly()
{
    AuthThrottle l_throttle(2, 5);
    l_throttle.recordFailure("abc123");
    l_throttle.recordFailure("abc123");
    QVERIFY(l_throttle.isLockedOut("abc123"));

    int l_remaining = l_throttle.remainingLockoutSeconds("abc123");
    QVERIFY(l_remaining > 0);
    QVERIFY(l_remaining <= 5);
}

void tst_AuthThrottle::setLimitsChangesThreshold()
{
    AuthThrottle l_throttle(5, 60);
    l_throttle.setLimits(2, 10);

    l_throttle.recordFailure("abc123");
    l_throttle.recordFailure("abc123");
    QVERIFY(l_throttle.isLockedOut("abc123"));
}

void tst_AuthThrottle::expiredLockoutResetsCountOnNextFailure()
{
    AuthThrottle l_throttle(2, 1);
    l_throttle.recordFailure("abc123");
    l_throttle.recordFailure("abc123");
    QVERIFY(l_throttle.isLockedOut("abc123"));

    QTest::qWait(1200);
    QVERIFY(!l_throttle.isLockedOut("abc123"));

    l_throttle.recordFailure("abc123");
    QVERIFY(!l_throttle.isLockedOut("abc123"));
}

void tst_AuthThrottle::separateIpidsAreIndependent()
{
    AuthThrottle l_throttle(2, 10);
    l_throttle.recordFailure("alice");
    l_throttle.recordFailure("alice");
    QVERIFY(l_throttle.isLockedOut("alice"));
    QVERIFY(!l_throttle.isLockedOut("bob"));

    l_throttle.recordFailure("bob");
    QVERIFY(!l_throttle.isLockedOut("bob"));
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_AuthThrottle)

#include "tst_auth_throttle.moc"
