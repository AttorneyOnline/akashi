// AI-generated: written by Claude.
#include "akashi/event.h"
#include "core/event_bus.h"

#include <QTest>

namespace tests {
namespace unittests {

class tst_EventBus : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void serviceId();
    void typedSubscribeAndGate();
    void gateCancels();
    void notifyAfterGate();
    void priorityOrder();
    void unsubscribe();
    void unsubscribeByOwner();
    void customEvents();
    void multipleEventTypes();
    void cancelStopsLaterBeforeHandlers();
    void gateAndNotifyPhases();
};

void tst_EventBus::serviceId()
{
    akashi::EventBus l_bus;
    QCOMPARE(l_bus.serviceId(), QStringLiteral("akashi.events"));
}

void tst_EventBus::typedSubscribeAndGate()
{
    akashi::EventBus l_bus;
    bool l_called = false;

    l_bus.subscribe<akashi::ICMessageEvent>(akashi::EventPhase::Before, 0,
        [&](akashi::ICMessageEvent &e) {
            l_called = true;
            QCOMPARE(e.message, QStringLiteral("Hello"));
        });

    akashi::ICMessageEvent l_event;
    l_event.message = QStringLiteral("Hello");
    bool l_allowed = l_bus.gate(l_event);

    QVERIFY(l_called);
    QVERIFY(l_allowed);
}

void tst_EventBus::gateCancels()
{
    akashi::EventBus l_bus;

    l_bus.subscribe<akashi::PlayerJoinedAreaEvent>(akashi::EventPhase::Before, 0,
        [](akashi::PlayerJoinedAreaEvent &e) {
            e.cancel(QStringLiteral("Area is locked"));
        });

    akashi::PlayerJoinedAreaEvent l_event;
    l_event.area_id = 1;
    bool l_allowed = l_bus.gate(l_event);

    QVERIFY(!l_allowed);
    QVERIFY(l_event.cancelled);
    QCOMPARE(l_event.cancel_reason, QStringLiteral("Area is locked"));
}

void tst_EventBus::notifyAfterGate()
{
    akashi::EventBus l_bus;
    bool l_before_called = false;
    bool l_after_called = false;

    l_bus.subscribe<akashi::ICMessageEvent>(akashi::EventPhase::Before, 0,
        [&](akashi::ICMessageEvent &) { l_before_called = true; });

    l_bus.subscribe<akashi::ICMessageEvent>(akashi::EventPhase::After, 0,
        [&](akashi::ICMessageEvent &) { l_after_called = true; });

    akashi::ICMessageEvent l_event;
    l_event.message = QStringLiteral("test");

    l_bus.gate(l_event);
    QVERIFY(l_before_called);
    QVERIFY(!l_after_called);

    l_bus.notify(l_event);
    QVERIFY(l_after_called);
}

void tst_EventBus::priorityOrder()
{
    akashi::EventBus l_bus;
    QStringList l_order;

    l_bus.subscribe<akashi::ICMessageEvent>(akashi::EventPhase::Before, 100,
        [&](akashi::ICMessageEvent &) { l_order.append(QStringLiteral("low")); });

    l_bus.subscribe<akashi::ICMessageEvent>(akashi::EventPhase::Before, 0,
        [&](akashi::ICMessageEvent &) { l_order.append(QStringLiteral("high")); });

    l_bus.subscribe<akashi::ICMessageEvent>(akashi::EventPhase::Before, 50,
        [&](akashi::ICMessageEvent &) { l_order.append(QStringLiteral("mid")); });

    akashi::ICMessageEvent l_event;
    l_bus.gate(l_event);

    QCOMPARE(l_order, QStringList({QStringLiteral("high"), QStringLiteral("mid"), QStringLiteral("low")}));
}

void tst_EventBus::unsubscribe()
{
    akashi::EventBus l_bus;
    int l_count = 0;

    int l_handle = l_bus.subscribe<akashi::ICMessageEvent>(akashi::EventPhase::After, 0,
        [&](akashi::ICMessageEvent &) { l_count++; });

    akashi::ICMessageEvent l_event;
    l_bus.notify(l_event);
    QCOMPARE(l_count, 1);

    l_bus.unsubscribe(l_handle);
    l_bus.notify(l_event);
    QCOMPARE(l_count, 1);
}

void tst_EventBus::unsubscribeByOwner()
{
    akashi::EventBus l_bus;
    int l_core_count = 0;
    int l_plugin_count = 0;

    l_bus.subscribe<akashi::ICMessageEvent>(akashi::EventPhase::After, 0,
        [&](akashi::ICMessageEvent &) { l_core_count++; }, QStringLiteral("core"));

    l_bus.subscribe<akashi::ICMessageEvent>(akashi::EventPhase::After, 0,
        [&](akashi::ICMessageEvent &) { l_plugin_count++; }, QStringLiteral("my-plugin"));

    akashi::ICMessageEvent l_event;
    l_bus.notify(l_event);
    QCOMPARE(l_core_count, 1);
    QCOMPARE(l_plugin_count, 1);

    l_bus.unsubscribeAll(QStringLiteral("my-plugin"));
    l_bus.notify(l_event);
    QCOMPARE(l_core_count, 2);
    QCOMPARE(l_plugin_count, 1);
}

void tst_EventBus::customEvents()
{
    akashi::EventBus l_bus;
    QVariantMap l_received;

    l_bus.subscribeCustom(QStringLiteral("weather.changed"), akashi::EventPhase::After,
        [&](const QVariantMap &payload) { l_received = payload; });

    QVariantMap l_payload;
    l_payload[QStringLiteral("condition")] = QStringLiteral("rain");
    l_payload[QStringLiteral("area_id")] = 3;
    l_bus.publishCustom(QStringLiteral("weather.changed"), l_payload);

    QCOMPARE(l_received.value(QStringLiteral("condition")).toString(), QStringLiteral("rain"));
    QCOMPARE(l_received.value(QStringLiteral("area_id")).toInt(), 3);
}

void tst_EventBus::multipleEventTypes()
{
    akashi::EventBus l_bus;
    int l_ic_count = 0;
    int l_ooc_count = 0;

    l_bus.subscribe<akashi::ICMessageEvent>(akashi::EventPhase::After, 0,
        [&](akashi::ICMessageEvent &) { l_ic_count++; });

    l_bus.subscribe<akashi::OOCMessageEvent>(akashi::EventPhase::After, 0,
        [&](akashi::OOCMessageEvent &) { l_ooc_count++; });

    akashi::ICMessageEvent l_ic;
    l_bus.notify(l_ic);
    l_bus.notify(l_ic);

    akashi::OOCMessageEvent l_ooc;
    l_bus.notify(l_ooc);

    QCOMPARE(l_ic_count, 2);
    QCOMPARE(l_ooc_count, 1);
}

void tst_EventBus::cancelStopsLaterBeforeHandlers()
{
    akashi::EventBus l_bus;
    bool l_second_called = false;

    l_bus.subscribe<akashi::PlayerJoinedAreaEvent>(akashi::EventPhase::Before, 0,
        [](akashi::PlayerJoinedAreaEvent &e) { e.cancel(); });

    l_bus.subscribe<akashi::PlayerJoinedAreaEvent>(akashi::EventPhase::Before, 100,
        [&](akashi::PlayerJoinedAreaEvent &) { l_second_called = true; });

    akashi::PlayerJoinedAreaEvent l_event;
    l_bus.gate(l_event);

    QVERIFY(!l_second_called);
}

void tst_EventBus::gateAndNotifyPhases()
{
    akashi::EventBus l_bus;
    QStringList l_trace;

    l_bus.subscribe<akashi::MusicChangedEvent>(akashi::EventPhase::Before, 0,
        [&](akashi::MusicChangedEvent &) { l_trace.append(QStringLiteral("before")); });

    l_bus.subscribe<akashi::MusicChangedEvent>(akashi::EventPhase::After, 0,
        [&](akashi::MusicChangedEvent &) { l_trace.append(QStringLiteral("after")); });

    akashi::MusicChangedEvent l_event;
    l_event.track_name = QStringLiteral("song.opus");

    QVERIFY(l_bus.gate(l_event));
    QCOMPARE(l_trace, QStringList({QStringLiteral("before")}));

    l_bus.notify(l_event);
    QCOMPARE(l_trace, QStringList({QStringLiteral("before"), QStringLiteral("after")}));
}

} // namespace unittests
} // namespace tests

QTEST_GUILESS_MAIN(tests::unittests::tst_EventBus)

#include "tst_event_bus.moc"
