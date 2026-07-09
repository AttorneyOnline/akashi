// AI-generated: written by Claude.
#include "akashi/scheduler.h"

#include <QTest>

namespace tests {
namespace unittests {

using namespace akashi;

class tst_Scheduler : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void dailyNextRunIsAlwaysAhead();
    void weeklyFiresOnTheRequestedDay();
    void dayWordsParse();
    void whenTextsParse();
    void onceFiresAndLeaves();
    void overdueOnceFiresImmediately();
    void runNowKeepsTheRhythm();
    void postponeDelaysTheRun();
    void cancelAllSweepsAnOwner();
    void scheduleRefusesUnusableJobs();
    void duplicateIdReplacesTheJob();
    void cancelUnknownIdLeavesOthersAlone();
    void cancelAllUnknownOwnerIsANoOp();
};

void tst_Scheduler::dailyNextRunIsAlwaysAhead()
{
    const QDateTime l_noon(QDate(2026, 7, 5), QTime(12, 0));

    // Later the same day stays today.
    QCOMPARE(*Schedule::daily(QTime(16, 30)).nextAfter(l_noon), QDateTime(QDate(2026, 7, 5), QTime(16, 30)));

    // Earlier in the day means tomorrow.
    QCOMPARE(*Schedule::daily(QTime(4, 0)).nextAfter(l_noon), QDateTime(QDate(2026, 7, 6), QTime(4, 0)));

    // Exactly now also means tomorrow, never zero.
    QCOMPARE(*Schedule::daily(QTime(12, 0)).nextAfter(l_noon), QDateTime(QDate(2026, 7, 6), QTime(12, 0)));
}

void tst_Scheduler::weeklyFiresOnTheRequestedDay()
{
    // 2026-07-05 is a Sunday.
    const QDateTime l_sunday_noon(QDate(2026, 7, 5), QTime(12, 0));

    // Wednesday afternoon is three days out.
    QCOMPARE(*Schedule::weekly(Qt::Wednesday, QTime(16, 0)).nextAfter(l_sunday_noon), QDateTime(QDate(2026, 7, 8), QTime(16, 0)));

    // A Sunday time already passed rolls a full week.
    QCOMPARE(*Schedule::weekly(Qt::Sunday, QTime(4, 0)).nextAfter(l_sunday_noon), QDateTime(QDate(2026, 7, 12), QTime(4, 0)));

    // A Sunday time still ahead stays today.
    QCOMPARE(*Schedule::weekly(Qt::Sunday, QTime(16, 0)).nextAfter(l_sunday_noon), QDateTime(QDate(2026, 7, 5), QTime(16, 0)));
}

void tst_Scheduler::dayWordsParse()
{
    QVERIFY(Schedule::fromDayWord(QStringLiteral("daily"), QTime(4, 0)).isValid());
    QVERIFY(Schedule::fromDayWord(QStringLiteral("Sunday"), QTime(4, 0)).isValid());
    QVERIFY(!Schedule::fromDayWord(QStringLiteral("fortnightly"), QTime(4, 0)).isValid());
    QVERIFY(!Schedule::fromDayWord(QStringLiteral("daily"), QTime()).isValid());

    const QDateTime l_sunday_noon(QDate(2026, 7, 5), QTime(12, 0));
    QCOMPARE(Schedule::fromDayWord(QStringLiteral("wednesday"), QTime(16, 0)).nextAfter(l_sunday_noon)->date(), QDate(2026, 7, 8));
}

void tst_Scheduler::whenTextsParse()
{
    const QDateTime l_sunday_noon(QDate(2026, 7, 5), QTime(12, 0)); // a Sunday

    // Compound durations add up, in any order and case.
    QCOMPARE(*parseWhen(QStringLiteral("90s"), l_sunday_noon), l_sunday_noon.addSecs(90));
    QCOMPARE(*parseWhen(QStringLiteral("1d12h"), l_sunday_noon), l_sunday_noon.addSecs(36 * 60 * 60));
    QCOMPARE(*parseWhen(QStringLiteral("30M1H"), l_sunday_noon), l_sunday_noon.addSecs(90 * 60));
    QCOMPARE(*parseWhen(QStringLiteral("1y1w1d1h1s"), l_sunday_noon),
             l_sunday_noon.addSecs(365ll * 86400 + 7 * 86400 + 86400 + 3600 + 1));

    // A weekday name is the coming midnight of that day; naming today
    // rolls a full week.
    QCOMPARE(*parseWhen(QStringLiteral("friday"), l_sunday_noon), QDateTime(QDate(2026, 7, 10), QTime(0, 0)));
    QCOMPARE(*parseWhen(QStringLiteral("Sunday"), l_sunday_noon), QDateTime(QDate(2026, 7, 12), QTime(0, 0)));

    // A calendar moment, with or without a time of day.
    QCOMPARE(*parseWhen(QStringLiteral("01.01.2028 18:00"), l_sunday_noon), QDateTime(QDate(2028, 1, 1), QTime(18, 0)));
    QCOMPARE(*parseWhen(QStringLiteral("24.12.2026"), l_sunday_noon), QDateTime(QDate(2026, 12, 24), QTime(0, 0)));

    // The past and nonsense read as nothing.
    QVERIFY(!parseWhen(QStringLiteral("01.01.2020 18:00"), l_sunday_noon).has_value());
    QVERIFY(!parseWhen(QStringLiteral("soon"), l_sunday_noon).has_value());
    QVERIFY(!parseWhen(QStringLiteral("0s"), l_sunday_noon).has_value());
    QVERIFY(!parseWhen(QString(), l_sunday_noon).has_value());
}

void tst_Scheduler::onceFiresAndLeaves()
{
    Scheduler l_scheduler;
    int l_fired = 0;
    QVERIFY(l_scheduler.schedule(QStringLiteral("blink"),
                                 Schedule::once(QDateTime::currentDateTime().addMSecs(80)),
                                 [&l_fired] { l_fired++; }));
    QVERIFY(l_scheduler.nextRunAt(QStringLiteral("blink")).has_value());

    QTRY_COMPARE_WITH_TIMEOUT(l_fired, 1, 2000);
    QVERIFY(!l_scheduler.nextRunAt(QStringLiteral("blink")).has_value());
}

void tst_Scheduler::overdueOnceFiresImmediately()
{
    // A one-shot from the past still runs - a sanction that should have
    // lifted while the server was down, lifts on the next tick.
    Scheduler l_scheduler;
    int l_fired = 0;
    QVERIFY(l_scheduler.schedule(QStringLiteral("missed"),
                                 Schedule::once(QDateTime::currentDateTime().addSecs(-3600)),
                                 [&l_fired] { l_fired++; }));
    QTRY_COMPARE_WITH_TIMEOUT(l_fired, 1, 2000);
}

void tst_Scheduler::runNowKeepsTheRhythm()
{
    Scheduler l_scheduler;
    int l_fired = 0;
    QVERIFY(l_scheduler.schedule(QStringLiteral("beat"), Schedule::daily(QTime(12, 0)), [&l_fired] { l_fired++; }));

    const auto l_before = l_scheduler.nextRunAt(QStringLiteral("beat"));
    QVERIFY(l_scheduler.runNow(QStringLiteral("beat")));
    QCOMPARE(l_fired, 1);
    QCOMPARE(l_scheduler.nextRunAt(QStringLiteral("beat")), l_before);

    QVERIFY(!l_scheduler.runNow(QStringLiteral("nonexistent")));
}

void tst_Scheduler::postponeDelaysTheRun()
{
    Scheduler l_scheduler;
    int l_fired = 0;
    QVERIFY(l_scheduler.schedule(QStringLiteral("shy"), Schedule::once(QDateTime::currentDateTime().addMSecs(50)), [&l_fired] { l_fired++; }, {}, [] { return true; }));

    QTest::qWait(400);
    QCOMPARE(l_fired, 0);
    const auto l_next = l_scheduler.nextRunAt(QStringLiteral("shy"));
    QVERIFY(l_next.has_value());
    QVERIFY(QDateTime::currentDateTime().secsTo(*l_next) > 25 * 60);
}

void tst_Scheduler::cancelAllSweepsAnOwner()
{
    Scheduler l_scheduler;
    l_scheduler.schedule(QStringLiteral("a"), Schedule::daily(QTime(12, 0)), [] {}, QStringLiteral("plugin.x"));
    l_scheduler.schedule(QStringLiteral("b"), Schedule::daily(QTime(12, 0)), [] {}, QStringLiteral("plugin.x"));
    l_scheduler.schedule(QStringLiteral("c"), Schedule::daily(QTime(12, 0)), [] {}, QStringLiteral("plugin.y"));

    l_scheduler.cancelAll(QStringLiteral("plugin.x"));
    QVERIFY(!l_scheduler.nextRunAt(QStringLiteral("a")).has_value());
    QVERIFY(!l_scheduler.nextRunAt(QStringLiteral("b")).has_value());
    QVERIFY(l_scheduler.nextRunAt(QStringLiteral("c")).has_value());

    l_scheduler.cancel(QStringLiteral("c"));
    QVERIFY(!l_scheduler.nextRunAt(QStringLiteral("c")).has_value());
}

void tst_Scheduler::scheduleRefusesUnusableJobs()
{
    Scheduler l_scheduler;

    // No id, a never-firing schedule, or no action.
    QVERIFY(!l_scheduler.schedule(QString(), Schedule::daily(QTime(12, 0)), [] {}));
    QVERIFY(!l_scheduler.schedule(QStringLiteral("never"), Schedule(), [] {}));
    QVERIFY(!l_scheduler.schedule(QStringLiteral("armless"), Schedule::daily(QTime(12, 0)), {}));

    QVERIFY(!l_scheduler.nextRunAt(QStringLiteral("never")).has_value());
    QVERIFY(!l_scheduler.nextRunAt(QStringLiteral("armless")).has_value());
}

void tst_Scheduler::duplicateIdReplacesTheJob()
{
    Scheduler l_scheduler;
    int l_old_fired = 0;
    int l_new_fired = 0;
    QVERIFY(l_scheduler.schedule(QStringLiteral("dup"), Schedule::daily(QTime(12, 0)), [&l_old_fired] { l_old_fired++; }));
    const auto l_old_next = l_scheduler.nextRunAt(QStringLiteral("dup"));

    // The same id again swaps out both the rhythm and the action.
    QVERIFY(l_scheduler.schedule(QStringLiteral("dup"), Schedule::daily(QTime(6, 0)), [&l_new_fired] { l_new_fired++; }));
    QVERIFY(l_scheduler.nextRunAt(QStringLiteral("dup")) != l_old_next);

    QVERIFY(l_scheduler.runNow(QStringLiteral("dup")));
    QCOMPARE(l_old_fired, 0);
    QCOMPARE(l_new_fired, 1);
}

void tst_Scheduler::cancelUnknownIdLeavesOthersAlone()
{
    Scheduler l_scheduler;
    l_scheduler.schedule(QStringLiteral("keeper"), Schedule::daily(QTime(12, 0)), [] {});

    l_scheduler.cancel(QStringLiteral("ghost"));
    QVERIFY(l_scheduler.nextRunAt(QStringLiteral("keeper")).has_value());
}

void tst_Scheduler::cancelAllUnknownOwnerIsANoOp()
{
    Scheduler l_scheduler;
    l_scheduler.schedule(QStringLiteral("a"), Schedule::daily(QTime(12, 0)), [] {}, QStringLiteral("plugin.x"));

    l_scheduler.cancelAll(QStringLiteral("plugin.z"));
    QVERIFY(l_scheduler.nextRunAt(QStringLiteral("a")).has_value());
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_Scheduler)

#include "tst_scheduler.moc"
