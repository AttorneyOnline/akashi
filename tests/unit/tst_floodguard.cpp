// AI-generated: written by Claude.
#include "world/floodguard.h"

#include <QTest>

namespace tests {
namespace unittests {

class tst_Floodguard : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void opensClosesAndReopens();
    void restartKeepsTheGateClosed();
    void zeroDurationClosesUntilTheNextEventLoopTurn();
};

void tst_Floodguard::opensClosesAndReopens()
{
    akashi::Floodguard l_guard;
    QVERIFY(l_guard.isMessageAllowed());

    l_guard.start(20);
    QVERIFY(!l_guard.isMessageAllowed());

    QTRY_VERIFY(l_guard.isMessageAllowed());
}

void tst_Floodguard::restartKeepsTheGateClosed()
{
    akashi::Floodguard l_guard;
    l_guard.start(50);
    QVERIFY(!l_guard.isMessageAllowed());

    // A second start while closed re-arms the timer instead of opening.
    l_guard.start(50);
    QVERIFY(!l_guard.isMessageAllowed());
    QTRY_VERIFY(l_guard.isMessageAllowed());
}

void tst_Floodguard::zeroDurationClosesUntilTheNextEventLoopTurn()
{
    akashi::Floodguard l_guard;

    // A zero duration still closes the gate; the timer reopens it on the
    // next event-loop turn, never within the same one.
    l_guard.start(0);
    QVERIFY(!l_guard.isMessageAllowed());
    QTRY_VERIFY(l_guard.isMessageAllowed());
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_Floodguard)

#include "tst_floodguard.moc"
