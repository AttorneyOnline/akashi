// AI-generated: written by Claude.
#include "akashi/area_rule.h"
#include "akashi/event.h"
#include "world/rule_registry.h"

#include <QTest>

namespace tests {
namespace unittests {

class tst_EventRegistry : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void observersRunInAscendingOrder();
    void observerTiesKeepRegistrationOrder();
    void observersFilterByEvent();
    void observerOwnerSweepRemovesOnlyThatOwner();
    void notifyingAnEmptyRegistryIsHarmless();
    void observersSeeTheContext();
    void customEventsRoundTripThroughObservers();
    void eventToMapCarriesEveryField();
    void eventToMapOnAFieldlessEvent();
    void eventIdsMatchTheRuleCatalog();
    void placelessEventsDispatchNoRulePhase();
};

void tst_EventRegistry::observersRunInAscendingOrder()
{
    akashi::RuleRegistry l_registry;
    QStringList l_trace;

    l_registry.registerObserver(
        QStringLiteral("music_changed"), 100,
        [&l_trace](const akashi::RuleContext &) { l_trace.append(QStringLiteral("late")); },
        QStringLiteral("plugin-a"));
    l_registry.registerObserver(
        QStringLiteral("music_changed"), 0,
        [&l_trace](const akashi::RuleContext &) { l_trace.append(QStringLiteral("early")); },
        QStringLiteral("plugin-a"));
    l_registry.registerObserver(
        QStringLiteral("music_changed"), 50,
        [&l_trace](const akashi::RuleContext &) { l_trace.append(QStringLiteral("mid")); },
        QStringLiteral("plugin-a"));

    l_registry.notifyObservers(QStringLiteral("music_changed"), {.player_state_id = 4, .client_session_id = 4, .area_id = 1, .floor_id = 0, .payload = {}, .services = nullptr});

    QCOMPARE(l_trace, QStringList({QStringLiteral("early"), QStringLiteral("mid"), QStringLiteral("late")}));
}

void tst_EventRegistry::observerTiesKeepRegistrationOrder()
{
    akashi::RuleRegistry l_registry;
    QStringList l_trace;

    l_registry.registerObserver(
        QStringLiteral("modcall"), 10,
        [&l_trace](const akashi::RuleContext &) { l_trace.append(QStringLiteral("first")); },
        QStringLiteral("plugin-a"));
    l_registry.registerObserver(
        QStringLiteral("modcall"), 10,
        [&l_trace](const akashi::RuleContext &) { l_trace.append(QStringLiteral("second")); },
        QStringLiteral("plugin-b"));
    l_registry.registerObserver(
        QStringLiteral("modcall"), 0,
        [&l_trace](const akashi::RuleContext &) { l_trace.append(QStringLiteral("front")); },
        QStringLiteral("plugin-c"));

    l_registry.notifyObservers(QStringLiteral("modcall"), {.player_state_id = 4, .client_session_id = 4, .area_id = 1, .floor_id = 0, .payload = {}, .services = nullptr});

    QCOMPARE(l_trace, QStringList({QStringLiteral("front"), QStringLiteral("first"), QStringLiteral("second")}));
}

void tst_EventRegistry::observersFilterByEvent()
{
    akashi::RuleRegistry l_registry;
    int l_music = 0;
    int l_ic = 0;

    l_registry.registerObserver(
        QStringLiteral("music_changed"), 0,
        [&l_music](const akashi::RuleContext &) { l_music++; },
        QStringLiteral("plugin-a"));
    l_registry.registerObserver(
        QStringLiteral("ic_message_sent"), 0,
        [&l_ic](const akashi::RuleContext &) { l_ic++; },
        QStringLiteral("plugin-a"));

    l_registry.notifyObservers(QStringLiteral("music_changed"), {.player_state_id = 4, .client_session_id = 4, .area_id = 1, .floor_id = 0, .payload = {}, .services = nullptr});
    l_registry.notifyObservers(QStringLiteral("music_changed"), {.player_state_id = 4, .client_session_id = 4, .area_id = 1, .floor_id = 0, .payload = {}, .services = nullptr});
    // An event nobody watches notifies nobody.
    l_registry.notifyObservers(QStringLiteral("ban_issued"), {.player_state_id = -1, .client_session_id = -1, .area_id = -1, .floor_id = -1, .payload = {}, .services = nullptr});

    QCOMPARE(l_music, 2);
    QCOMPARE(l_ic, 0);
}

void tst_EventRegistry::observerOwnerSweepRemovesOnlyThatOwner()
{
    akashi::RuleRegistry l_registry;
    int l_a = 0;
    int l_b = 0;

    l_registry.registerObserver(
        QStringLiteral("player_left"), 0,
        [&l_a](const akashi::RuleContext &) { l_a++; },
        QStringLiteral("plugin-a"));
    l_registry.registerObserver(
        QStringLiteral("player_left"), 5,
        [&l_b](const akashi::RuleContext &) { l_b++; },
        QStringLiteral("plugin-b"));

    l_registry.unregisterObservers(QStringLiteral("plugin-a"));
    l_registry.notifyObservers(QStringLiteral("player_left"), {.player_state_id = 4, .client_session_id = 4, .area_id = 1, .floor_id = 0, .payload = {}, .services = nullptr});

    QCOMPARE(l_a, 0);
    QCOMPARE(l_b, 1);

    // Sweeping an unknown owner is harmless.
    l_registry.unregisterObservers(QStringLiteral("plugin-x"));
    l_registry.notifyObservers(QStringLiteral("player_left"), {.player_state_id = 4, .client_session_id = 4, .area_id = 1, .floor_id = 0, .payload = {}, .services = nullptr});
    QCOMPARE(l_b, 2);
}

void tst_EventRegistry::notifyingAnEmptyRegistryIsHarmless()
{
    // Nothing registered at all: notify and sweep fall through quietly,
    // and the registry still works afterwards.
    akashi::RuleRegistry l_registry;
    l_registry.notifyObservers(QStringLiteral("modcall"), {.player_state_id = -1, .client_session_id = -1, .area_id = -1, .floor_id = -1, .payload = {}, .services = nullptr});
    l_registry.unregisterObservers(QStringLiteral("plugin-x"));

    int l_count = 0;
    l_registry.registerObserver(
        QStringLiteral("modcall"), 0,
        [&l_count](const akashi::RuleContext &) { l_count++; },
        QStringLiteral("plugin-a"));
    l_registry.notifyObservers(QStringLiteral("modcall"), {.player_state_id = -1, .client_session_id = -1, .area_id = -1, .floor_id = -1, .payload = {}, .services = nullptr});
    QCOMPARE(l_count, 1);
}

void tst_EventRegistry::observersSeeTheContext()
{
    akashi::RuleRegistry l_registry;
    QString l_seen;
    int l_slot = -1;
    int l_session = -1;

    l_registry.registerObserver(
        QStringLiteral("ooc_message_sent"), 0,
        [&](const akashi::RuleContext &f_ctx) {
            l_seen = f_ctx.payload.value("message").toString();
            l_slot = f_ctx.player_state_id;
            l_session = f_ctx.client_session_id;
        },
        QStringLiteral("plugin-a"));

    // Distinct ids prove the slot and the session travel independently.
    l_registry.notifyObservers(QStringLiteral("ooc_message_sent"),
                               {.player_state_id = 7, .client_session_id = 9, .area_id = 2, .floor_id = 1, .payload = {{QStringLiteral("message"), QStringLiteral("hello court")}}, .services = nullptr});

    QCOMPARE(l_seen, QString("hello court"));
    QCOMPARE(l_slot, 7);
    QCOMPARE(l_session, 9);
}

void tst_EventRegistry::customEventsRoundTripThroughObservers()
{
    // A plugin-invented id travels the same path as a core event, and the
    // payload keeps its variant types on the way through.
    akashi::RuleRegistry l_registry;
    QVariantMap l_received;

    l_registry.registerObserver(
        QStringLiteral("weather.changed"), 0,
        [&l_received](const akashi::RuleContext &f_ctx) { l_received = f_ctx.payload; },
        QStringLiteral("plugin-a"));

    l_registry.notifyObservers(QStringLiteral("weather.changed"),
                               {.player_state_id = -1, .client_session_id = -1, .area_id = -1, .floor_id = -1, .payload = {{QStringLiteral("condition"), QStringLiteral("rain")}, {QStringLiteral("area_id"), 3}}, .services = nullptr});

    QCOMPARE(l_received.value(QStringLiteral("condition")).toString(), QString("rain"));
    QCOMPARE(l_received.value(QStringLiteral("area_id")).toInt(), 3);
}

void tst_EventRegistry::eventToMapCarriesEveryField()
{
    const akashi::ModcallEvent l_event{
        .client_session_id = 4,
        .player_state_id = 4,
        .area_id = 2,
        .area_name = QStringLiteral("Courtroom"),
        .char_name = QStringLiteral("Phoenix"),
        .ooc_name = QStringLiteral("nick"),
        .ipid = QStringLiteral("1.2.3.4"),
        .reason = QStringLiteral("Objection spam")};

    const QVariantMap l_map = akashi::eventToMap(l_event);

    QCOMPARE(l_map.size(), 8);
    QCOMPARE(l_map.value("client_session_id").toInt(), 4);
    QCOMPARE(l_map.value("player_state_id").toInt(), 4);
    QCOMPARE(l_map.value("area_id").toInt(), 2);
    QCOMPARE(l_map.value("area_name").toString(), QString("Courtroom"));
    QCOMPARE(l_map.value("char_name").toString(), QString("Phoenix"));
    QCOMPARE(l_map.value("ooc_name").toString(), QString("nick"));
    QCOMPARE(l_map.value("ipid").toString(), QString("1.2.3.4"));
    QCOMPARE(l_map.value("reason").toString(), QString("Objection spam"));

    // Non-string types keep their type through the metaobject.
    const akashi::PlayerDisconnectedEvent l_gone{.client_session_id = 9, .player_state_id = 9, .was_joined = true};
    const QVariantMap l_gone_map = akashi::eventToMap(l_gone);
    QCOMPARE(l_gone_map.value("was_joined").toBool(), true);
    QCOMPARE(l_gone_map.value("client_session_id").toInt(), 9);
    QCOMPARE(l_gone_map.value("player_state_id").toInt(), 9);
    QCOMPARE(l_gone_map.size(), 7);
}

void tst_EventRegistry::eventToMapOnAFieldlessEvent()
{
    const akashi::ConfigReloadedEvent l_event;
    QVERIFY(akashi::eventToMap(l_event).isEmpty());
}

void tst_EventRegistry::eventIdsMatchTheRuleCatalog()
{
    // Where the concept maps to a rule event, the strings match.
    QCOMPARE(akashi::PlayerJoinedAreaEvent::id, akashi::AreaEvents::PlayerJoined);
    QCOMPARE(akashi::PlayerLeftAreaEvent::id, akashi::AreaEvents::PlayerLeft);
    QCOMPARE(akashi::ICMessageEvent::id, akashi::AreaEvents::IcMessageSent);
    QCOMPARE(akashi::OOCMessageEvent::id, akashi::AreaEvents::OocMessageSent);
    QCOMPARE(akashi::MusicChangedEvent::id, akashi::AreaEvents::MusicChanged);
    QCOMPARE(akashi::EvidencePresentedEvent::id, akashi::AreaEvents::EvidencePresented);

    // The placeless and unmapped ones carry their own ids.
    QCOMPARE(akashi::AreaChangedEvent::id, QStringLiteral("area_changed"));
    QCOMPARE(akashi::ModcallEvent::id, QStringLiteral("modcall"));
    QCOMPARE(akashi::BanIssuedEvent::id, QStringLiteral("ban_issued"));
    QCOMPARE(akashi::KickIssuedEvent::id, QStringLiteral("kick_issued"));
    QCOMPARE(akashi::PlayerDisconnectedEvent::id, QStringLiteral("player_disconnected"));
    QCOMPARE(akashi::ConfigReloadedEvent::id, QStringLiteral("config_reloaded"));
}

void tst_EventRegistry::placelessEventsDispatchNoRulePhase()
{
    // Placeless events sit in the catalog so /addrule can refuse them,
    // but no rule phase ever dispatches; only observers hear them.
    const QStringList l_placeless = {
        akashi::ModcallEvent::id,
        akashi::BanIssuedEvent::id,
        akashi::KickIssuedEvent::id,
        akashi::PlayerDisconnectedEvent::id,
        akashi::ConfigReloadedEvent::id,
    };
    for (const QString &l_event : l_placeless) {
        const auto l_before = akashi::RuleRegistry::eventSupportsPhase(l_event, akashi::RulePhase::Before);
        const auto l_after = akashi::RuleRegistry::eventSupportsPhase(l_event, akashi::RulePhase::After);
        const auto l_transform = akashi::RuleRegistry::eventSupportsPhase(l_event, akashi::RulePhase::Transform);
        QVERIFY2(l_before.has_value() && !*l_before, qPrintable(l_event));
        QVERIFY2(l_after.has_value() && !*l_after, qPrintable(l_event));
        QVERIFY2(l_transform.has_value() && !*l_transform, qPrintable(l_event));
    }
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_EventRegistry)

#include "tst_event_registry.moc"
