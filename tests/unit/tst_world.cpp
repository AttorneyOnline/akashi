// AI-generated: written by Claude.
#include "world/area.h"
#include "world/area_rules.h"
#include "world/floor.h"

#include <QSignalSpy>
#include <QTest>

namespace tests {
namespace unittests {

class tst_World : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void areaReportsRealChangesOnly();
    void areaTracksPlayersAndCharacters();
    void areaManagesOwnersAndInvitations();
    void visibilityDefaultsToTheWholeFloor();
    void floorDefaultsMatchToday();
    void rulesApplyToTheirScopeOnly();
    void firstBlockingRuleWins();
    void areaRulesOverwriteFloorRulesOfTheSameName();
    void beforeRulesGateWithoutTouchingAfterRules();
    void afterRulesActOnTheWorld();
    void pluginRuleObjectsRegisterAndReleaseOnUnload();
    void unregisteringAnOwnerRemovesItsRules();
};

void tst_World::areaReportsRealChangesOnly()
{
    akashi::Area l_area(0, "Courtroom", 0, 0);
    QSignalSpy l_status(&l_area, &akashi::Area::statusChanged);
    QSignalSpy l_lock(&l_area, &akashi::Area::lockStateChanged);
    QSignalSpy l_count(&l_area, &akashi::Area::playerCountChanged);

    l_area.setStatus("CASING");
    l_area.setStatus("CASING"); // no change, no signal
    l_area.setLockState(akashi::Area::LockState::Spectatable);
    l_area.addPlayer(4);
    l_area.addPlayer(4); // already present, no signal

    QCOMPARE(l_status.size(), 1);
    QCOMPARE(l_lock.size(), 1);
    QCOMPARE(l_count.size(), 1);
    QCOMPARE(l_count[0][0].toInt(), 1);

    // The protocol never limited the status to fixed values: any line
    // works, cut off at 30 characters.
    const QString l_custom = "Chapter 3 - The Cursed Courtroom Returns";
    l_area.setStatus(l_custom);
    QCOMPARE(l_area.status(), l_custom.left(30));
    QCOMPARE(l_area.status().size(), 30);
}

void tst_World::areaTracksPlayersAndCharacters()
{
    akashi::Area l_area(0, "Courtroom", 0, 0);
    l_area.addPlayer(4);
    l_area.addPlayer(7);
    l_area.removePlayer(4);

    QCOMPARE(l_area.players(), QVector<int>({7}));
    QCOMPARE(l_area.playerCount(), 1);

    QVERIFY(l_area.takeCharacter(2));
    QVERIFY(!l_area.takeCharacter(2)); // taken here already
    l_area.releaseCharacter(2);
    QVERIFY(l_area.takeCharacter(2));
}

void tst_World::areaManagesOwnersAndInvitations()
{
    akashi::Area l_area(0, "Courtroom", 0, 0);
    QSignalSpy l_owners(&l_area, &akashi::Area::ownersChanged);

    l_area.addOwner(4);
    l_area.addOwner(4); // already an owner, no signal
    QVERIFY(l_area.hasOwners());
    QVERIFY(l_area.removeOwner(4));
    QVERIFY(!l_area.removeOwner(4)); // no longer an owner
    QVERIFY(!l_area.hasOwners());
    QCOMPARE(l_owners.size(), 2);

    QVERIFY(l_area.invite(7));
    QVERIFY(!l_area.invite(7)); // already invited
    QVERIFY(l_area.uninvite(7));
    QVERIFY(!l_area.uninvite(7));
}

void tst_World::visibilityDefaultsToTheWholeFloor()
{
    akashi::Area l_area(3, "Basement", 1, 2);

    // Empty set = every area on the floor; a map may narrow it.
    QVERIFY(l_area.visibleAreas().isEmpty());
    l_area.setVisibleAreas({3, 4});
    QCOMPARE(l_area.visibleAreas(), QSet<int>({3, 4}));

    QCOMPARE(l_area.floorId(), 1);
    QCOMPARE(l_area.x(), 2);
}

void tst_World::floorDefaultsMatchToday()
{
    akashi::Floor l_floor;

    // A fresh floor behaves like today's server: characters unique per area.
    QCOMPARE(l_floor.character_policy, akashi::Floor::CharacterPolicy::UniquePerArea);
    QVERIFY(l_floor.area_ids.isEmpty());
}

void tst_World::rulesApplyToTheirScopeOnly()
{
    akashi::AreaRuleRegistry l_rules;
    // Rules are a per-area setup: an area can be stricter than its peer.
    l_rules.registerAreaRule("no-chat-in-area-2", akashi::AreaEvent::MessageSent, akashi::RulePhase::Before, 2,
                             [](const akashi::AreaEventDetails &) { return akashi::RuleVerdict{false, "This area is silent."}; },
                             "test");
    // A floor manages its areas, so its rule covers every area under it.
    l_rules.registerFloorRule("no-evidence-on-floor-1", akashi::AreaEvent::EvidencePresented, akashi::RulePhase::Before, 1,
                              [](const akashi::AreaEventDetails &) { return akashi::RuleVerdict{false, "No evidence here."}; },
                              "test");

    // The area rule fires only in its own area, not in its peer.
    QVERIFY(!l_rules.check(akashi::AreaEvent::MessageSent, akashi::RulePhase::Before, {4, 2, 0, "hello"}).allowed);
    QVERIFY(l_rules.check(akashi::AreaEvent::MessageSent, akashi::RulePhase::Before, {4, 3, 0, "hello"}).allowed);
    // The floor rule covers every area of its floor, and only that floor.
    QVERIFY(!l_rules.check(akashi::AreaEvent::EvidencePresented, akashi::RulePhase::Before, {4, 9, 1, "Knife"}).allowed);
    QVERIFY(l_rules.check(akashi::AreaEvent::EvidencePresented, akashi::RulePhase::Before, {4, 9, 0, "Knife"}).allowed);
    // Other events pass through untouched.
    QVERIFY(l_rules.check(akashi::AreaEvent::PlayerJoined, akashi::RulePhase::Before, {4, 2, 0, {}}).allowed);
}

void tst_World::firstBlockingRuleWins()
{
    akashi::AreaRuleRegistry l_rules;
    // Area rules are the highest granularity, so they answer first.
    l_rules.registerAreaRule("area-rule", akashi::AreaEvent::PlayerJoined, akashi::RulePhase::Before, 0,
                             [](const akashi::AreaEventDetails &) { return akashi::RuleVerdict{false, "the area says no"}; },
                             "test");
    l_rules.registerFloorRule("floor-rule", akashi::AreaEvent::PlayerJoined, akashi::RulePhase::Before, 0,
                              [](const akashi::AreaEventDetails &) { return akashi::RuleVerdict{false, "the floor says no"}; },
                              "test");

    QCOMPARE(l_rules.check(akashi::AreaEvent::PlayerJoined, akashi::RulePhase::Before, {4, 0, 0, {}}).reason, QString("the area says no"));
}

void tst_World::areaRulesOverwriteFloorRulesOfTheSameName()
{
    akashi::AreaRuleRegistry l_rules;
    // The floor forbids evidence everywhere...
    l_rules.registerFloorRule("no-evidence", akashi::AreaEvent::EvidencePresented, akashi::RulePhase::Before, 0,
                              [](const akashi::AreaEventDetails &) { return akashi::RuleVerdict{false, "No evidence on this floor."}; },
                              "test");
    // ...but area 2 overwrites that rule with its own, allowing it.
    l_rules.registerAreaRule("no-evidence", akashi::AreaEvent::EvidencePresented, akashi::RulePhase::Before, 2,
                             [](const akashi::AreaEventDetails &) { return akashi::RuleVerdict{}; },
                             "test");

    QVERIFY(l_rules.check(akashi::AreaEvent::EvidencePresented, akashi::RulePhase::Before, {4, 2, 0, "Knife"}).allowed);
    QVERIFY(!l_rules.check(akashi::AreaEvent::EvidencePresented, akashi::RulePhase::Before, {4, 3, 0, "Knife"}).allowed);
}

void tst_World::beforeRulesGateWithoutTouchingAfterRules()
{
    akashi::AreaRuleRegistry l_rules;
    // The gate refuses the change while it is still a request, so the
    // world never applies it and the client never sees a flicker of
    // applied-then-reverted state.
    l_rules.registerAreaRule("one-character-only", akashi::AreaEvent::PlayerJoined, akashi::RulePhase::Before, 0,
                             [](const akashi::AreaEventDetails &) { return akashi::RuleVerdict{false, "Only one character selection allowed."}; },
                             "test");
    bool l_after_ran = false;
    l_rules.registerAreaRule("welcome", akashi::AreaEvent::PlayerJoined, akashi::RulePhase::After, 0,
                             [&l_after_ran](const akashi::AreaEventDetails &) {
                                 l_after_ran = true;
                                 return akashi::RuleVerdict{};
                             },
                             "test");

    const akashi::RuleVerdict l_verdict = l_rules.check(akashi::AreaEvent::PlayerJoined, akashi::RulePhase::Before, {4, 0, 0, {}});

    // The gate blocked, and checking the gate did not run any After rule -
    // the change never happened, so there is nothing to react to.
    QVERIFY(!l_verdict.allowed);
    QCOMPARE(l_verdict.reason, QString("Only one character selection allowed."));
    QVERIFY(!l_after_ran);
}

void tst_World::afterRulesActOnTheWorld()
{
    akashi::Area l_vault(2, "Vault", 0, 2);
    l_vault.setLockState(akashi::Area::LockState::Locked);

    akashi::AreaRuleRegistry l_rules;
    // A reaction: saying the magic words in the hallway unlocks the vault.
    // The function captures what it operates on.
    l_rules.registerAreaRule("magic-words", akashi::AreaEvent::MessageSent, akashi::RulePhase::After, 1,
                             [&l_vault](const akashi::AreaEventDetails &f_details) {
                                 if (f_details.text == "open sesame") {
                                     l_vault.setLockState(akashi::Area::LockState::Free);
                                 }
                                 return akashi::RuleVerdict{};
                             },
                             "test");
    bool l_watcher_ran = false;
    l_rules.registerFloorRule("watcher", akashi::AreaEvent::MessageSent, akashi::RulePhase::After, 0,
                              [&l_watcher_ran](const akashi::AreaEventDetails &) {
                                  l_watcher_ran = true;
                                  return akashi::RuleVerdict{};
                              },
                              "test");

    // The message went through; every reaction attached to the hook runs.
    l_rules.check(akashi::AreaEvent::MessageSent, akashi::RulePhase::After, {4, 1, 0, "open sesame"});

    QCOMPARE(l_vault.lockState(), akashi::Area::LockState::Free);
    QVERIFY(l_watcher_ran);
}

// A rule the way a plugin would ship it: a subclass of the SDK contract.
class CursedDoorRule : public akashi::AreaRule
{
  public:
    akashi::RuleVerdict onEvent(const akashi::AreaEventDetails &f_details) override
    {
        if (f_details.text == "Cursed Key") {
            return {};
        }
        return {false, "The door only opens for the Cursed Key."};
    }
};

void tst_World::pluginRuleObjectsRegisterAndReleaseOnUnload()
{
    akashi::AreaRuleRegistry l_rules;
    std::shared_ptr<akashi::AreaRule> l_rule = std::make_shared<CursedDoorRule>();
    std::weak_ptr<akashi::AreaRule> l_alive = l_rule;

    l_rules.registerAreaRule("cursed-door", akashi::AreaEvent::EvidencePresented, akashi::RulePhase::Before, 0, l_rule, "plugin-doors");
    l_rule.reset();

    // The registry keeps the plugin's object alive and runs it like any rule.
    QVERIFY(!l_alive.expired());
    QVERIFY(!l_rules.check(akashi::AreaEvent::EvidencePresented, akashi::RulePhase::Before, {4, 0, 0, "Rusty Crowbar"}).allowed);
    QVERIFY(l_rules.check(akashi::AreaEvent::EvidencePresented, akashi::RulePhase::Before, {4, 0, 0, "Cursed Key"}).allowed);

    // Unloading the plugin unregisters and releases the object.
    l_rules.unregisterAll("plugin-doors");
    QCOMPARE(l_rules.ruleCount(), 0);
    QVERIFY(l_alive.expired());
}

void tst_World::unregisteringAnOwnerRemovesItsRules()
{
    akashi::AreaRuleRegistry l_rules;
    l_rules.registerAreaRule("mine", akashi::AreaEvent::PlayerJoined, akashi::RulePhase::Before, 0,
                             [](const akashi::AreaEventDetails &) { return akashi::RuleVerdict{false, "no"}; },
                             "plugin-a");
    l_rules.registerFloorRule("also-mine", akashi::AreaEvent::PlayerLeft, akashi::RulePhase::After, 0,
                              [](const akashi::AreaEventDetails &) { return akashi::RuleVerdict{false, "no"}; },
                              "plugin-a");
    l_rules.registerAreaRule("someone-elses", akashi::AreaEvent::PlayerJoined, akashi::RulePhase::Before, 0,
                             [](const akashi::AreaEventDetails &) { return akashi::RuleVerdict{false, "still no"}; },
                             "plugin-b");

    l_rules.unregisterAll("plugin-a");

    QCOMPARE(l_rules.ruleCount(), 1);
    QCOMPARE(l_rules.check(akashi::AreaEvent::PlayerJoined, akashi::RulePhase::Before, {4, 0, 0, {}}).reason, QString("still no"));
    QVERIFY(l_rules.check(akashi::AreaEvent::PlayerLeft, akashi::RulePhase::After, {4, 0, 0, {}}).allowed);
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_World)

#include "tst_world.moc"
