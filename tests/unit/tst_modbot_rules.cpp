// AI-generated: written by Claude.
#include "modbot_rules.h"
#include "modbot_worker.h"

#include <QTemporaryDir>
#include <QTest>

namespace tests {
namespace unittests {

using namespace akashi::modbot;

class tst_ModbotRules : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void escalationLadderClimbsWithHistory();
    void floodWindowSlides();
    void repeatedMessageTriggers();
    void screenDropsAccumulateToAVerdict();
    void modcallFloodOnlyEverWarns();
    void quietPeriodAnswersABurstWithOneVerdict();
    void humanModerationFeedsHistoryOnly();
    void queueDropsOldestWhenFull();
    void incidentStoreCountsPerIpid();

  private:
    static Config testConfig();
    static Event chatEvent(qint64 f_epoch, const QString &f_message = QStringLiteral("hello"));
};

Config tst_ModbotRules::testConfig()
{
    Config l_config;
    l_config.flood_messages = 3;
    l_config.flood_seconds = 5;
    l_config.repeat_messages = 3;
    l_config.strike_screen_drops = 2;
    l_config.strike_window_seconds = 60;
    l_config.modcall_flood_count = 2;
    l_config.modcall_flood_seconds = 30;
    l_config.mute_minutes = 5;
    l_config.mute_minutes_repeat = 30;
    l_config.kick_after_incidents = 4;
    return l_config;
}

Event tst_ModbotRules::chatEvent(qint64 f_epoch, const QString &f_message)
{
    Event l_event;
    l_event.kind = Event::Kind::IcMessage;
    l_event.epoch = f_epoch;
    l_event.client_session_id = 7;
    l_event.ipid = QStringLiteral("abcd1234");
    l_event.message = f_message;
    return l_event;
}

void tst_ModbotRules::escalationLadderClimbsWithHistory()
{
    // The same offense answers differently as the history grows: a warning
    // first, a short mute, a long mute, and finally a kick.
    const QList<QPair<int, Verdict::Action>> l_ladder = {
        {0, Verdict::Action::Warn},
        {1, Verdict::Action::Mute},
        {2, Verdict::Action::Mute},
        {4, Verdict::Action::Kick},
    };
    for (const auto &l_step : l_ladder) {
        Rules l_rules(testConfig());
        QList<Verdict> l_verdicts;
        for (int i = 0; i < 3; i++) {
            l_verdicts.append(l_rules.analyze(chatEvent(100, QString::number(i)), l_step.first));
        }
        QCOMPARE(l_verdicts.size(), 1);
        QCOMPARE(l_verdicts.first().action, l_step.second);
        QCOMPARE(l_verdicts.first().ipid, QStringLiteral("abcd1234"));
    }

    // The mute lengths climb with the history too.
    Rules l_first(testConfig());
    l_first.analyze(chatEvent(100, "a"), 1);
    l_first.analyze(chatEvent(100, "b"), 1);
    QCOMPARE(l_first.analyze(chatEvent(100, "c"), 1).first().mute_minutes, 5);

    Rules l_repeat(testConfig());
    l_repeat.analyze(chatEvent(100, "a"), 2);
    l_repeat.analyze(chatEvent(100, "b"), 2);
    QCOMPARE(l_repeat.analyze(chatEvent(100, "c"), 2).first().mute_minutes, 30);
}

void tst_ModbotRules::floodWindowSlides()
{
    // The same number of messages spread wider than the window is no flood.
    Rules l_rules(testConfig());
    QVERIFY(l_rules.analyze(chatEvent(100, "a"), 0).isEmpty());
    QVERIFY(l_rules.analyze(chatEvent(110, "b"), 0).isEmpty());
    QVERIFY(l_rules.analyze(chatEvent(120, "c"), 0).isEmpty());
    QVERIFY(l_rules.analyze(chatEvent(130, "d"), 0).isEmpty());
}

void tst_ModbotRules::repeatedMessageTriggers()
{
    // Repeats are spam even when they are slow enough to pass the flood
    // window; case and surrounding spaces do not disguise them.
    Rules l_rules(testConfig());
    QVERIFY(l_rules.analyze(chatEvent(100, "Buy my stuff"), 0).isEmpty());
    QVERIFY(l_rules.analyze(chatEvent(110, "BUY MY STUFF"), 0).isEmpty());
    const QList<Verdict> l_verdicts = l_rules.analyze(chatEvent(120, "  buy my stuff  "), 0);
    QCOMPARE(l_verdicts.size(), 1);
    QCOMPARE(l_verdicts.first().reason, QStringLiteral("repeating the same message"));
}

void tst_ModbotRules::screenDropsAccumulateToAVerdict()
{
    Rules l_rules(testConfig());
    Event l_drop = chatEvent(100, "blocked content");
    l_drop.kind = Event::Kind::ScreenDrop;

    QVERIFY(l_rules.analyze(l_drop, 0).isEmpty());
    l_drop.epoch = 130;
    const QList<Verdict> l_verdicts = l_rules.analyze(l_drop, 0);
    QCOMPARE(l_verdicts.size(), 1);
    QCOMPARE(l_verdicts.first().reason, QStringLiteral("posting blocked content"));
}

void tst_ModbotRules::modcallFloodOnlyEverWarns()
{
    // Even a heavy history never escalates a modcall flood past a warning:
    // punishing the help channel would teach people not to use it.
    Rules l_rules(testConfig());
    Event l_call = chatEvent(100, "help");
    l_call.kind = Event::Kind::Modcall;

    QVERIFY(l_rules.analyze(l_call, 10).isEmpty());
    l_call.epoch = 105;
    const QList<Verdict> l_verdicts = l_rules.analyze(l_call, 10);
    QCOMPARE(l_verdicts.size(), 1);
    QCOMPARE(l_verdicts.first().action, Verdict::Action::Warn);
}

void tst_ModbotRules::quietPeriodAnswersABurstWithOneVerdict()
{
    // A burst keeps flooding after the verdict; the actor is not judged
    // again until the quiet period passes.
    Rules l_rules(testConfig());
    QList<Verdict> l_verdicts;
    for (int i = 0; i < 8; i++) {
        l_verdicts.append(l_rules.analyze(chatEvent(100 + i, QString::number(i)), 0));
    }
    QCOMPARE(l_verdicts.size(), 1);

    // Once the quiet period ends, a fresh flood is judged again.
    for (int i = 0; i < 3; i++) {
        l_verdicts.append(l_rules.analyze(chatEvent(200 + i, QStringLiteral("x%1").arg(i)), 0));
    }
    QCOMPARE(l_verdicts.size(), 2);
}

void tst_ModbotRules::humanModerationFeedsHistoryOnly()
{
    Rules l_rules(testConfig());
    Event l_ban = chatEvent(100, "spamming");
    l_ban.kind = Event::Kind::BanIssued;
    QVERIFY(l_rules.analyze(l_ban, 0).isEmpty());

    Event l_kick = l_ban;
    l_kick.kind = Event::Kind::KickIssued;
    QVERIFY(l_rules.analyze(l_kick, 0).isEmpty());
}

void tst_ModbotRules::queueDropsOldestWhenFull()
{
    BoundedEventQueue l_queue(3);
    QVERIFY(l_queue.enqueue(chatEvent(1, "one")));
    QVERIFY(l_queue.enqueue(chatEvent(2, "two")));
    QVERIFY(l_queue.enqueue(chatEvent(3, "three")));

    // The fourth entry costs the oldest one, never the enqueuer's time.
    QVERIFY(!l_queue.enqueue(chatEvent(4, "four")));
    QCOMPARE(l_queue.droppedCount(), 1);

    const QList<Event> l_batch = l_queue.waitAndDrain();
    QCOMPARE(l_batch.size(), 3);
    QCOMPARE(l_batch.first().message, QStringLiteral("two"));
    QCOMPARE(l_batch.last().message, QStringLiteral("four"));

    // A closed queue answers with emptiness and swallows late arrivals.
    l_queue.close();
    QVERIFY(l_queue.waitAndDrain().isEmpty());
    QVERIFY(l_queue.enqueue(chatEvent(5, "five")));
    QVERIFY(l_queue.waitAndDrain().isEmpty());
}

void tst_ModbotRules::incidentStoreCountsPerIpid()
{
    QTemporaryDir l_dir;
    IncidentStore l_store;
    QVERIFY(l_store.open(l_dir.filePath(QStringLiteral("incidents.db")), QStringLiteral("tst_modbot_store")));

    QCOMPARE(l_store.incidentCount(QStringLiteral("abcd1234")), 0);
    l_store.record(100, QStringLiteral("abcd1234"), QStringLiteral("warn"), QStringLiteral("flood"));
    l_store.record(200, QStringLiteral("abcd1234"), QStringLiteral("mute"), QStringLiteral("flood"));
    l_store.record(300, QStringLiteral("beef5678"), QStringLiteral("warn"), QStringLiteral("spam"));

    QCOMPARE(l_store.incidentCount(QStringLiteral("abcd1234")), 2);
    QCOMPARE(l_store.incidentCount(QStringLiteral("beef5678")), 1);
    l_store.close();
}

} // namespace unittests
} // namespace tests

QTEST_GUILESS_MAIN(tests::unittests::tst_ModbotRules)

#include "tst_modbot_rules.moc"
