// AI-generated: written by Claude.
#include "akashi/service_registry.h"
#include "core/rule_actions.h"
#include "world/area.h"
#include "world/evidence_store.h"
#include "world/floor.h"
#include "world/rule_registry.h"
#include "world/world.h"

#include <QFile>
#include <QRegularExpression>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
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
    void areaRulesOverrideFloorRulesOfSameAction();
    void evidenceStoreNumbersItemsPerViewer();
    void beforeRulesGateWithoutTouchingAfterRules();
    void afterRulesActOnTheWorld();
    void pluginBeforeRuleObjectsRegisterAndReleaseOnUnload();
    void unregisteringAnOwnerRemovesItsRules();
    void pluginUnloadSweepTakesDirectAndActionBuiltRules();
    void eventCatalogDeclaresPhases();
    void transformsMergeChangedKeysInSequence();
    void areaTransformsSuppressFloorTransformsOfSameAction();
    void emptyTransformResultLeavesThePayloadAlone();
    void transformActionsRegisterBuildAndReport();
    void transformRulesSweepOnPluginUnload();
    void eventCatalogDeclaresTransformAndPlacement();
    void ruleListingIncludesTransformEntries();
    void checkCharacterHoldsAClaimPerArea();
    void checkCharacterHoldsAClaimPerFloor();
    void crossPhaseBuildsReturnNothingForEveryPairing();
    void duplicateActionNamesAreRefused();
    void unknownOwnerSweepsAreHarmless();
    void dispatchSkipsEventsNothingMatches();
    void transformsMayIntroduceKeysThePayloadNeverHad();
    void evidenceStoreIgnoresIndexesOutOfBounds();
    void taggedDescriptionOnlyRewritesUntaggedHiddenItems();
    void createRefusesBlankNamesAndUnknownFloors();
    void renamesRefuseInvalidIdsAndNames();
    void removeAreaRefusesItsGuards();
    void removeFloorRefusesItsGuards();
    void configRulesSkipUnknownActionsAndWrongPhases();
    void configRulesForUnknownScopesAttachNothing();
};

void tst_World::areaReportsRealChangesOnly()
{
    akashi::Area l_area(0, "Courtroom", 0, 0);
    QSignalSpy l_status(&l_area, &akashi::Area::statusChanged);
    QSignalSpy l_lock(&l_area, &akashi::Area::lockStateChanged);
    QSignalSpy l_count(&l_area, &akashi::Area::playerCountChanged);

    l_area.setStatus("CASING");
    l_area.setStatus("CASING");
    l_area.setLockState(akashi::Area::LockState::Spectatable);
    l_area.addPlayer(4);
    l_area.addPlayer(4);

    QCOMPARE(l_status.size(), 1);
    QCOMPARE(l_lock.size(), 1);
    QCOMPARE(l_count.size(), 1);
    QCOMPARE(l_count[0][0].toInt(), 1);

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
    QVERIFY(!l_area.takeCharacter(2));
    l_area.releaseCharacter(2);
    QVERIFY(l_area.takeCharacter(2));
}

void tst_World::areaManagesOwnersAndInvitations()
{
    akashi::Area l_area(0, "Courtroom", 0, 0);
    QSignalSpy l_owners(&l_area, &akashi::Area::ownersChanged);

    l_area.addOwner(4);
    l_area.addOwner(4);
    QVERIFY(l_area.hasOwners());
    // Becoming an owner includes the invitation.
    QVERIFY(l_area.invited().contains(4));

    // The area frees itself when its last owner leaves it locked.
    l_area.setLockState(akashi::Area::LockState::Locked);
    QCOMPARE(l_area.removeOwner(4), akashi::Area::OwnerRemoval::RemovedAndUnlocked);
    QVERIFY(!l_area.hasOwners());
    QCOMPARE(l_area.lockState(), akashi::Area::LockState::Free);
    QVERIFY(!l_area.invited().contains(4));
    // Removing them again is a refusal, not a quiet success.
    QCOMPARE(l_area.removeOwner(4), akashi::Area::OwnerRemoval::NotAnOwner);
    QCOMPARE(l_owners.size(), 2);

    QVERIFY(l_area.invite(7));
    QVERIFY(!l_area.invite(7));
    QVERIFY(l_area.uninvite(7));
    QVERIFY(!l_area.uninvite(7));
}

void tst_World::visibilityDefaultsToTheWholeFloor()
{
    akashi::Area l_area(3, "Basement", 1, 2);

    QVERIFY(l_area.visibleAreas().isEmpty());
    l_area.setVisibleAreas({3, 4});
    QCOMPARE(l_area.visibleAreas(), QSet<int>({3, 4}));

    QCOMPARE(l_area.floorId(), 1);
    QCOMPARE(l_area.x(), 2);
}

void tst_World::floorDefaultsMatchToday()
{
    akashi::Floor l_floor;

    QVERIFY(l_floor.area_ids.isEmpty());
    QVERIFY(l_floor.before_rules.isEmpty());
    QVERIFY(l_floor.after_rules.isEmpty());
    QVERIFY(l_floor.transform_rules.isEmpty());
}

void tst_World::rulesApplyToTheirScopeOnly()
{
    // Area rule on area 2 for IC, floor rule on floor 1 for evidence.
    QVector<akashi::BeforeRuleEntry> l_area2_rules = {
        {akashi::AreaEvents::IcMessageSent, QStringLiteral("block_ic"),
         [](const akashi::RuleContext &) { return akashi::RuleVerdict{false, "This area is silent."}; },
         "test"},
    };
    QVector<akashi::BeforeRuleEntry> l_floor1_rules = {
        {akashi::AreaEvents::EvidencePresented, QStringLiteral("block_evidence"),
         [](const akashi::RuleContext &) { return akashi::RuleVerdict{false, "No evidence here."}; },
         "test"},
    };
    QVector<akashi::BeforeRuleEntry> l_empty;

    // Area 2 blocks IC.
    QVERIFY(!akashi::RuleRegistry::checkBefore(akashi::AreaEvents::IcMessageSent, {.player_state_id = 4, .client_session_id = 4, .area_id = 2, .floor_id = 0, .payload = {}, .services = nullptr},
                                               l_area2_rules, l_empty)
                 .allowed);
    // Area 3 has no rules — IC allowed.
    QVERIFY(akashi::RuleRegistry::checkBefore(akashi::AreaEvents::IcMessageSent, {.player_state_id = 4, .client_session_id = 4, .area_id = 3, .floor_id = 0, .payload = {}, .services = nullptr},
                                              l_empty, l_empty)
                .allowed);
    // Floor 1 blocks evidence.
    QVERIFY(!akashi::RuleRegistry::checkBefore(akashi::AreaEvents::EvidencePresented, {.player_state_id = 4, .client_session_id = 4, .area_id = 9, .floor_id = 1, .payload = {}, .services = nullptr},
                                               l_empty, l_floor1_rules)
                 .allowed);
    // Floor 0 has no rules — evidence allowed.
    QVERIFY(akashi::RuleRegistry::checkBefore(akashi::AreaEvents::EvidencePresented, {.player_state_id = 4, .client_session_id = 4, .area_id = 9, .floor_id = 0, .payload = {}, .services = nullptr},
                                              l_empty, l_empty)
                .allowed);
    // PlayerJoined has no rules anywhere — allowed.
    QVERIFY(akashi::RuleRegistry::checkBefore(akashi::AreaEvents::PlayerJoined, {.player_state_id = 4, .client_session_id = 4, .area_id = 2, .floor_id = 0, .payload = {}, .services = nullptr},
                                              l_area2_rules, l_empty)
                .allowed);
}

void tst_World::firstBlockingRuleWins()
{
    QVector<akashi::BeforeRuleEntry> l_area_rules = {
        {akashi::AreaEvents::PlayerJoined, QStringLiteral("check_gate"),
         [](const akashi::RuleContext &) { return akashi::RuleVerdict{false, "the area says no"}; },
         "test"},
    };
    QVector<akashi::BeforeRuleEntry> l_floor_rules = {
        {akashi::AreaEvents::PlayerJoined, QStringLiteral("check_character"),
         [](const akashi::RuleContext &) { return akashi::RuleVerdict{false, "the floor says no"}; },
         "test"},
    };

    const auto l_verdict = akashi::RuleRegistry::checkBefore(akashi::AreaEvents::PlayerJoined, {.player_state_id = 4, .client_session_id = 4, .area_id = 0, .floor_id = 0, .payload = {}, .services = nullptr},
                                                             l_area_rules, l_floor_rules);
    QVERIFY(!l_verdict.allowed);
    QCOMPARE(l_verdict.reason, QString("the area says no"));
}

void tst_World::areaRulesOverrideFloorRulesOfSameAction()
{
    QVector<akashi::BeforeRuleEntry> l_floor_rules = {
        {akashi::AreaEvents::EvidencePresented, QStringLiteral("check_evidence"),
         [](const akashi::RuleContext &) { return akashi::RuleVerdict{false, "No evidence on this floor."}; },
         "test"},
        {akashi::AreaEvents::EvidencePresented, QStringLiteral("check_probe"),
         [](const akashi::RuleContext &) { return akashi::RuleVerdict{}; },
         "test"},
    };
    QVector<akashi::BeforeRuleEntry> l_area_rules = {
        {akashi::AreaEvents::EvidencePresented, QStringLiteral("check_evidence"),
         [](const akashi::RuleContext &) { return akashi::RuleVerdict{}; },
         "test"},
    };

    // Area overrides check_evidence (allows), floor check_probe still runs (allows).
    QVERIFY(akashi::RuleRegistry::checkBefore(akashi::AreaEvents::EvidencePresented, {.player_state_id = 4, .client_session_id = 4, .area_id = 2, .floor_id = 0, .payload = {}, .services = nullptr},
                                              l_area_rules, l_floor_rules)
                .allowed);
    // Without area rules, floor check_evidence blocks.
    QVector<akashi::BeforeRuleEntry> l_empty;
    QVERIFY(!akashi::RuleRegistry::checkBefore(akashi::AreaEvents::EvidencePresented, {.player_state_id = 4, .client_session_id = 4, .area_id = 3, .floor_id = 0, .payload = {}, .services = nullptr},
                                               l_empty, l_floor_rules)
                 .allowed);

    // Floor check_probe blocks, area only overrides check_evidence.
    QVector<akashi::BeforeRuleEntry> l_floor_rules2 = {
        {akashi::AreaEvents::EvidencePresented, QStringLiteral("check_evidence"),
         [](const akashi::RuleContext &) { return akashi::RuleVerdict{false, "floor blocks"}; },
         "test"},
        {akashi::AreaEvents::EvidencePresented, QStringLiteral("check_probe"),
         [](const akashi::RuleContext &) { return akashi::RuleVerdict{false, "no permission"}; },
         "test"},
    };

    const auto l_verdict = akashi::RuleRegistry::checkBefore(akashi::AreaEvents::EvidencePresented, {.player_state_id = 4, .client_session_id = 4, .area_id = 2, .floor_id = 0, .payload = {}, .services = nullptr},
                                                             l_area_rules, l_floor_rules2);
    QVERIFY(!l_verdict.allowed);
    QCOMPARE(l_verdict.reason, QString("no permission"));
}

void tst_World::evidenceStoreNumbersItemsPerViewer()
{
    akashi::EvidenceStore l_store;
    l_store.setAccess(akashi::EvidenceStore::Access::HiddenCm);
    l_store.append({"Sword", "<owner=def>\nA sword.", "sword.png"});
    l_store.append({"Photo", "Everyone sees this.", "photo.png"});
    l_store.append({"Contract", "<owner=pro>\nSigned.", "contract.png"});
    l_store.append({"Key", "<owner=all>\nRevealed.", "key.png"});

    QCOMPARE(l_store.visibleItems(true, "wit").size(), 4);
    QCOMPARE(l_store.visibleItems(false, "def").size(), 3);
    QCOMPARE(l_store.visibleItems(false, "pro").size(), 3);

    QCOMPARE(l_store.itemIndexByVisibleIndex(2, false, "def"), 1);
    QCOMPARE(l_store.itemIndexByVisibleIndex(3, false, "def"), 3);
    QCOMPARE(l_store.itemIndexByVisibleIndex(2, false, "pro"), 2);

    QCOMPARE(l_store.visibleIndexByItemIndex(2, false, "pro"), 2);
    QCOMPARE(l_store.visibleIndexByItemIndex(2, false, "def"), 0);

    for (int l_position = 1; l_position <= 3; l_position++) {
        const int l_real = l_store.itemIndexByVisibleIndex(l_position, false, "def");
        QCOMPARE(l_store.visibleIndexByItemIndex(l_real, false, "def"), l_position);
    }

    QCOMPARE(l_store.itemIndexByVisibleIndex(4, false, "def"), -1);

    l_store.setAccess(akashi::EvidenceStore::Access::FreeForAll);
    QCOMPARE(l_store.itemIndexByVisibleIndex(3, false, "def"), 2);
}

void tst_World::beforeRulesGateWithoutTouchingAfterRules()
{
    QVector<akashi::BeforeRuleEntry> l_before = {
        {akashi::AreaEvents::PlayerJoined, QStringLiteral("check_character"),
         [](const akashi::RuleContext &) { return akashi::RuleVerdict{false, "Only one character selection allowed."}; },
         "test"},
    };
    bool l_after_ran = false;
    QVector<akashi::AfterRuleEntry> l_after = {
        {akashi::AreaEvents::PlayerJoined, QStringLiteral("send_welcome"),
         [&l_after_ran](const akashi::RuleContext &) { l_after_ran = true; },
         "test"},
    };

    const akashi::RuleVerdict l_verdict = akashi::RuleRegistry::checkBefore(
        akashi::AreaEvents::PlayerJoined, {.player_state_id = 4, .client_session_id = 4, .area_id = 0, .floor_id = 0, .payload = {}, .services = nullptr},
        l_before, {});

    // The caller's contract: after-rules only run when the gate allows.
    if (l_verdict.allowed) {
        akashi::RuleRegistry::runAfter(
            akashi::AreaEvents::PlayerJoined, {.player_state_id = 4, .client_session_id = 4, .area_id = 0, .floor_id = 0, .payload = {}, .services = nullptr},
            l_after, {});
    }

    QVERIFY(!l_verdict.allowed);
    QCOMPARE(l_verdict.reason, QString("Only one character selection allowed."));
    QVERIFY(!l_after_ran);
}

void tst_World::afterRulesActOnTheWorld()
{
    akashi::Area l_vault(2, "Vault", 0, 2);
    l_vault.setLockState(akashi::Area::LockState::Locked);

    QVector<akashi::AfterRuleEntry> l_area_after = {
        {akashi::AreaEvents::IcMessageSent, QStringLiteral("magic_unlock"),
         [&l_vault](const akashi::RuleContext &f_ctx) {
             if (f_ctx.payload.value("message").toString() == "open sesame")
                 l_vault.setLockState(akashi::Area::LockState::Free);
         },
         "test"},
    };

    akashi::RuleRegistry::runAfter(akashi::AreaEvents::IcMessageSent,
                                   {.player_state_id = 4, .client_session_id = 4, .area_id = 1, .floor_id = 0, .payload = {{QStringLiteral("message"), QStringLiteral("open sesame")}}, .services = nullptr},
                                   l_area_after, {});

    QCOMPARE(l_vault.lockState(), akashi::Area::LockState::Free);
}

class CursedDoorRule : public akashi::BeforeRule
{
  public:
    akashi::RuleVerdict onEvent(const akashi::RuleContext &f_ctx) override
    {
        if (f_ctx.payload.value("evidence_name").toString() == "Cursed Key")
            return {};
        return {false, "The door only opens for the Cursed Key."};
    }
};

void tst_World::pluginBeforeRuleObjectsRegisterAndReleaseOnUnload()
{
    std::shared_ptr<akashi::BeforeRule> l_rule = std::make_shared<CursedDoorRule>();
    std::weak_ptr<akashi::BeforeRule> l_alive = l_rule;

    QVector<akashi::BeforeRuleEntry> l_area_rules = {
        {akashi::AreaEvents::EvidencePresented, QStringLiteral("cursed_door"),
         [l_rule](const akashi::RuleContext &f_ctx) { return l_rule->onEvent(f_ctx); },
         "plugin-doors"},
    };
    l_rule.reset();

    QVERIFY(!l_alive.expired());
    QVERIFY(!akashi::RuleRegistry::checkBefore(akashi::AreaEvents::EvidencePresented,
                                               {.player_state_id = 4, .client_session_id = 4, .area_id = 0, .floor_id = 0, .payload = {{QStringLiteral("evidence_name"), QStringLiteral("Rusty Crowbar")}}, .services = nullptr},
                                               l_area_rules, {})
                 .allowed);
    QVERIFY(akashi::RuleRegistry::checkBefore(akashi::AreaEvents::EvidencePresented,
                                              {.player_state_id = 4, .client_session_id = 4, .area_id = 0, .floor_id = 0, .payload = {{QStringLiteral("evidence_name"), QStringLiteral("Cursed Key")}}, .services = nullptr},
                                              l_area_rules, {})
                .allowed);

    // Clearing the vector releases the shared_ptr.
    l_area_rules.clear();
    QVERIFY(l_alive.expired());
}

void tst_World::unregisteringAnOwnerRemovesItsRules()
{
    // Rules live on area/floor objects. Unregistering by owner is a sweep.
    akashi::Floor l_floor;
    l_floor.before_rules.append({akashi::AreaEvents::PlayerJoined, QStringLiteral("block_a"),
                                 [](const akashi::RuleContext &) { return akashi::RuleVerdict{false, "no"}; },
                                 "plugin-a"});
    l_floor.after_rules.append({akashi::AreaEvents::PlayerLeft, QStringLiteral("watch_a"),
                                [](const akashi::RuleContext &) {},
                                "plugin-a"});
    l_floor.before_rules.append({akashi::AreaEvents::PlayerJoined, QStringLiteral("block_b"),
                                 [](const akashi::RuleContext &) { return akashi::RuleVerdict{false, "still no"}; },
                                 "plugin-b"});

    // Remove plugin-a's rules.
    auto l_remove = [](auto &f_vec, const QString &f_owner) {
        for (int i = f_vec.size() - 1; i >= 0; --i) {
            if (f_vec.at(i).owner_id == f_owner)
                f_vec.removeAt(i);
        }
    };
    l_remove(l_floor.before_rules, QStringLiteral("plugin-a"));
    l_remove(l_floor.after_rules, QStringLiteral("plugin-a"));

    QCOMPARE(l_floor.before_rules.size() + l_floor.after_rules.size(), 1);
    QCOMPARE(akashi::RuleRegistry::checkBefore(akashi::AreaEvents::PlayerJoined, {.player_state_id = 4, .client_session_id = 4, .area_id = 0, .floor_id = 0, .payload = {}, .services = nullptr},
                                               {}, l_floor.before_rules)
                 .reason,
             QString("still no"));
}

void tst_World::pluginUnloadSweepTakesDirectAndActionBuiltRules()
{
    akashi::RuleRegistry l_registry;
    l_registry.registerBeforeAction(
        QStringLiteral("cursed_gate"),
        [](akashi::ServiceRegistry &, const QVariantMap &) -> akashi::BeforeRuleFunction {
            return [](const akashi::RuleContext &) { return akashi::RuleVerdict{false, "no"}; };
        },
        QStringLiteral("plugin-doors"));

    akashi::Floor l_floor;
    // A rule the plugin attached itself.
    l_floor.before_rules.append({akashi::AreaEvents::PlayerJoined, QStringLiteral("watch"),
                                 [](const akashi::RuleContext &) { return akashi::RuleVerdict{}; },
                                 QStringLiteral("plugin-doors")});
    // A rule built from the plugin's action, attached by config.
    l_floor.before_rules.append({akashi::AreaEvents::EvidencePresented, QStringLiteral("cursed_gate"),
                                 [](const akashi::RuleContext &) { return akashi::RuleVerdict{false, "no"}; },
                                 QStringLiteral("config")});
    // A core rule that must survive the sweep.
    l_floor.after_rules.append({akashi::AreaEvents::PlayerJoined, QStringLiteral("send_message"),
                                [](const akashi::RuleContext &) {},
                                QStringLiteral("core")});

    const QStringList l_owned = l_registry.actionsOwnedBy(QStringLiteral("plugin-doors"));
    QCOMPARE(l_owned, QStringList({QStringLiteral("cursed_gate")}));

    const QSet<QString> l_actions(l_owned.begin(), l_owned.end());
    const int l_removed = akashi::RuleRegistry::removeRules(QStringLiteral("plugin-doors"), l_actions,
                                                            l_floor.before_rules, l_floor.after_rules);
    QCOMPARE(l_removed, 2);
    QVERIFY(l_floor.before_rules.isEmpty());
    QCOMPARE(l_floor.after_rules.size(), 1);
    QCOMPARE(l_floor.after_rules[0].owner_id, QString("core"));
}

void tst_World::eventCatalogDeclaresPhases()
{
    // ic_message_sent dispatches both phases.
    const auto l_ic_before = akashi::RuleRegistry::eventSupportsPhase(akashi::AreaEvents::IcMessageSent, akashi::RulePhase::Before);
    const auto l_ic_after = akashi::RuleRegistry::eventSupportsPhase(akashi::AreaEvents::IcMessageSent, akashi::RulePhase::After);
    QVERIFY(l_ic_before.has_value() && *l_ic_before);
    QVERIFY(l_ic_after.has_value() && *l_ic_after);

    // player_left only fires once the player is gone.
    const auto l_left_before = akashi::RuleRegistry::eventSupportsPhase(akashi::AreaEvents::PlayerLeft, akashi::RulePhase::Before);
    const auto l_left_after = akashi::RuleRegistry::eventSupportsPhase(akashi::AreaEvents::PlayerLeft, akashi::RulePhase::After);
    QVERIFY(l_left_before.has_value() && !*l_left_before);
    QVERIFY(l_left_after.has_value() && *l_left_after);

    // character_changed gained its before phase with the character verb.
    const auto l_char_before = akashi::RuleRegistry::eventSupportsPhase(akashi::AreaEvents::CharacterChanged, akashi::RulePhase::Before);
    const auto l_char_after = akashi::RuleRegistry::eventSupportsPhase(akashi::AreaEvents::CharacterChanged, akashi::RulePhase::After);
    QVERIFY(l_char_before.has_value() && *l_char_before);
    QVERIFY(l_char_after.has_value() && *l_char_after);

    // Unknown ids are plugin/custom events; the catalog says nothing.
    QVERIFY(!akashi::RuleRegistry::eventSupportsPhase(QStringLiteral("myplugin.custom"), akashi::RulePhase::Before).has_value());
    QVERIFY(!akashi::RuleRegistry::eventSupportsPhase(QStringLiteral("myplugin.custom"), akashi::RulePhase::After).has_value());
}

void tst_World::transformsMergeChangedKeysInSequence()
{
    QVector<akashi::TransformRuleEntry> l_floor_rules = {
        {akashi::AreaEvents::IcMessageSent, QStringLiteral("lowercase"),
         [](const akashi::RuleContext &f_ctx) {
             QVariantMap l_changes;
             l_changes.insert(QStringLiteral("message"), f_ctx.payload.value("message").toString().toLower());
             return l_changes;
         },
         "test"},
        {akashi::AreaEvents::IcMessageSent, QStringLiteral("punctuate"),
         [](const akashi::RuleContext &f_ctx) {
             QVariantMap l_changes;
             l_changes.insert(QStringLiteral("message"), f_ctx.payload.value("message").toString() + "!");
             return l_changes;
         },
         "test"},
    };

    const QVariantMap l_result = akashi::RuleRegistry::runTransforms(
        akashi::AreaEvents::IcMessageSent,
        {.player_state_id = 4, .client_session_id = 4, .area_id = 1, .floor_id = 0, .payload = {{QStringLiteral("message"), QStringLiteral("HELLO")}, {QStringLiteral("char_name"), QStringLiteral("Phoenix")}}, .services = nullptr},
        {}, l_floor_rules);

    // The second transform saw the first one's rewrite.
    QCOMPARE(l_result.value("message").toString(), QString("hello!"));
    // Untouched keys survive.
    QCOMPARE(l_result.value("char_name").toString(), QString("Phoenix"));
}

void tst_World::areaTransformsSuppressFloorTransformsOfSameAction()
{
    QVector<akashi::TransformRuleEntry> l_area_rules = {
        {akashi::AreaEvents::IcMessageSent, QStringLiteral("censor"),
         [](const akashi::RuleContext &) {
             return QVariantMap{{QStringLiteral("message"), QStringLiteral("area censored")}};
         },
         "test"},
    };
    QVector<akashi::TransformRuleEntry> l_floor_rules = {
        {akashi::AreaEvents::IcMessageSent, QStringLiteral("censor"),
         [](const akashi::RuleContext &) {
             return QVariantMap{{QStringLiteral("message"), QStringLiteral("floor censored")}};
         },
         "test"},
        {akashi::AreaEvents::IcMessageSent, QStringLiteral("tag"),
         [](const akashi::RuleContext &f_ctx) {
             return QVariantMap{{QStringLiteral("message"), f_ctx.payload.value("message").toString() + " [tagged]"}};
         },
         "test"},
    };

    const QVariantMap l_result = akashi::RuleRegistry::runTransforms(
        akashi::AreaEvents::IcMessageSent,
        {.player_state_id = 4, .client_session_id = 4, .area_id = 2, .floor_id = 0, .payload = {{QStringLiteral("message"), QStringLiteral("original")}}, .services = nullptr},
        l_area_rules, l_floor_rules);

    // The area's censor ran and suppressed the floor's; the floor's tag
    // still ran and saw the area's rewrite - area before floor.
    QCOMPARE(l_result.value("message").toString(), QString("area censored [tagged]"));

    // Without area rules, the floor's own censor applies.
    const QVariantMap l_floor_only = akashi::RuleRegistry::runTransforms(
        akashi::AreaEvents::IcMessageSent,
        {.player_state_id = 4, .client_session_id = 4, .area_id = 3, .floor_id = 0, .payload = {{QStringLiteral("message"), QStringLiteral("original")}}, .services = nullptr},
        {}, l_floor_rules);
    QCOMPARE(l_floor_only.value("message").toString(), QString("floor censored [tagged]"));
}

void tst_World::emptyTransformResultLeavesThePayloadAlone()
{
    bool l_ran = false;
    QVector<akashi::TransformRuleEntry> l_floor_rules = {
        {akashi::AreaEvents::OocMessageSent, QStringLiteral("watchful"),
         [&l_ran](const akashi::RuleContext &) {
             l_ran = true;
             return QVariantMap{};
         },
         "test"},
        // A transform on another event never runs.
        {akashi::AreaEvents::IcMessageSent, QStringLiteral("other_event"),
         [](const akashi::RuleContext &) {
             return QVariantMap{{QStringLiteral("message"), QStringLiteral("wrong")}};
         },
         "test"},
    };

    const QVariantMap l_payload = {{QStringLiteral("message"), QStringLiteral("untouched")},
                                   {QStringLiteral("char_name"), QStringLiteral("Edgeworth")}};
    const QVariantMap l_result = akashi::RuleRegistry::runTransforms(
        akashi::AreaEvents::OocMessageSent, {.player_state_id = 4, .client_session_id = 4, .area_id = 1, .floor_id = 0, .payload = l_payload, .services = nullptr}, {}, l_floor_rules);

    QVERIFY(l_ran);
    QCOMPARE(l_result, l_payload);
}

void tst_World::transformActionsRegisterBuildAndReport()
{
    akashi::RuleRegistry l_registry;
    akashi::ServiceRegistry l_services;

    l_registry.registerTransformAction(
        QStringLiteral("uppercase"),
        [](akashi::ServiceRegistry &, const QVariantMap &f_args) -> akashi::TransformRuleFunction {
            const QString l_key = f_args.value(QStringLiteral("key"), QStringLiteral("message")).toString();
            return [l_key](const akashi::RuleContext &f_ctx) {
                return QVariantMap{{l_key, f_ctx.payload.value(l_key).toString().toUpper()}};
            };
        },
        QStringLiteral("plugin-shout"));
    l_registry.registerBeforeAction(
        QStringLiteral("gate"),
        [](akashi::ServiceRegistry &, const QVariantMap &) -> akashi::BeforeRuleFunction {
            return [](const akashi::RuleContext &) { return akashi::RuleVerdict{}; };
        },
        QStringLiteral("plugin-shout"));

    QVERIFY(l_registry.hasAction(QStringLiteral("uppercase")));
    QCOMPARE(l_registry.actionPhase(QStringLiteral("uppercase")), akashi::RulePhase::Transform);

    const auto l_fn = l_registry.buildTransform(QStringLiteral("uppercase"), l_services,
                                                {{QStringLiteral("key"), QStringLiteral("message")}});
    QVERIFY(l_fn.has_value());
    const QVariantMap l_changes = (*l_fn)({.player_state_id = 4, .client_session_id = 4, .area_id = 0, .floor_id = 0, .payload = {{QStringLiteral("message"), QStringLiteral("objection")}}, .services = nullptr});
    QCOMPARE(l_changes.value("message").toString(), QString("OBJECTION"));

    // Phase mismatches and unknown names build nothing.
    QVERIFY(!l_registry.buildBefore(QStringLiteral("uppercase"), l_services, {}).has_value());
    QVERIFY(!l_registry.buildAfter(QStringLiteral("uppercase"), l_services, {}).has_value());
    QVERIFY(!l_registry.buildTransform(QStringLiteral("gate"), l_services, {}).has_value());
    QVERIFY(!l_registry.buildTransform(QStringLiteral("missing"), l_services, {}).has_value());

    // The owner sweep takes transform action definitions too.
    const QStringList l_owned = l_registry.actionsOwnedBy(QStringLiteral("plugin-shout"));
    QCOMPARE(l_owned.size(), 2);
    QVERIFY(l_owned.contains(QStringLiteral("uppercase")));
    l_registry.unregisterActions(QStringLiteral("plugin-shout"));
    QVERIFY(!l_registry.hasAction(QStringLiteral("uppercase")));
    QVERIFY(!l_registry.hasAction(QStringLiteral("gate")));
}

void tst_World::transformRulesSweepOnPluginUnload()
{
    const auto l_noop = [](const akashi::RuleContext &) { return QVariantMap{}; };

    akashi::Floor l_floor;
    // A transform the plugin attached itself.
    l_floor.transform_rules.append({akashi::AreaEvents::IcMessageSent, QStringLiteral("curse"),
                                    l_noop, QStringLiteral("plugin-medieval")});
    // A transform built from the plugin's action, attached by config.
    l_floor.transform_rules.append({akashi::AreaEvents::IcMessageSent, QStringLiteral("owned_action"),
                                    l_noop, QStringLiteral("config")});
    // A core transform that must survive the sweep.
    l_floor.transform_rules.append({akashi::AreaEvents::IcMessageSent, QStringLiteral("keep"),
                                    l_noop, QStringLiteral("core")});
    // A before rule of the same owner sweeps alongside.
    l_floor.before_rules.append({akashi::AreaEvents::PlayerJoined, QStringLiteral("watch"),
                                 [](const akashi::RuleContext &) { return akashi::RuleVerdict{}; },
                                 QStringLiteral("plugin-medieval")});

    const int l_removed = akashi::RuleRegistry::removeRules(
        QStringLiteral("plugin-medieval"), {QStringLiteral("owned_action")},
        l_floor.before_rules, l_floor.after_rules, l_floor.transform_rules);

    QCOMPARE(l_removed, 3);
    QVERIFY(l_floor.before_rules.isEmpty());
    QCOMPARE(l_floor.transform_rules.size(), 1);
    QCOMPARE(l_floor.transform_rules[0].owner_id, QString("core"));
}

void tst_World::eventCatalogDeclaresTransformAndPlacement()
{
    // Every placed event dispatches at least one phase; a placeless entry
    // dispatches none. The IC verb dispatches transforms today; the other
    // placed entries flip as their verbs wire the phase in.
    for (const akashi::AreaEventInfo &l_info : akashi::areaEventCatalog()) {
        if (l_info.placed) {
            QVERIFY2(l_info.supports_before || l_info.supports_after, qPrintable(l_info.id));
        }
        else {
            QVERIFY2(!l_info.supports_before && !l_info.supports_after && !l_info.supports_transform, qPrintable(l_info.id));
        }
        if (l_info.supports_transform) {
            QCOMPARE(l_info.id, akashi::AreaEvents::IcMessageSent);
        }
    }

    const auto l_ic = akashi::RuleRegistry::eventSupportsPhase(akashi::AreaEvents::IcMessageSent, akashi::RulePhase::Transform);
    QVERIFY(l_ic.has_value() && *l_ic);
    const auto l_ooc = akashi::RuleRegistry::eventSupportsPhase(akashi::AreaEvents::OocMessageSent, akashi::RulePhase::Transform);
    QVERIFY(l_ooc.has_value() && !*l_ooc);

    // Unknown ids stay unknown for the transform phase too.
    QVERIFY(!akashi::RuleRegistry::eventSupportsPhase(QStringLiteral("myplugin.custom"), akashi::RulePhase::Transform).has_value());
}

void tst_World::ruleListingIncludesTransformEntries()
{
    const auto l_noop = [](const akashi::RuleContext &) { return QVariantMap{}; };

    QVector<akashi::TransformRuleEntry> l_area_transforms = {
        {akashi::AreaEvents::IcMessageSent, QStringLiteral("apply_filter"), l_noop, QStringLiteral("command")},
    };
    QVector<akashi::TransformRuleEntry> l_floor_transforms = {
        // Same event:action as the area's - overridden, so hidden.
        {akashi::AreaEvents::IcMessageSent, QStringLiteral("apply_filter"), l_noop, QStringLiteral("core")},
        {akashi::AreaEvents::IcMessageSent, QStringLiteral("strip_shouts"), l_noop, QStringLiteral("core")},
    };

    const auto l_rules = akashi::RuleRegistry::rulesForArea({}, {}, l_area_transforms, {}, {}, l_floor_transforms);

    QCOMPARE(l_rules.size(), 2);
    QCOMPARE(l_rules[0].action, QString("apply_filter"));
    QCOMPARE(l_rules[0].phase, akashi::RulePhase::Transform);
    QVERIFY(!l_rules[0].is_floor_rule);
    QCOMPARE(l_rules[1].action, QString("strip_shouts"));
    QCOMPARE(l_rules[1].phase, akashi::RulePhase::Transform);
    QVERIFY(l_rules[1].is_floor_rule);
}

// A world of two floors with two areas on the first: Gate (0) and Annex (1)
// on Yard, Loft (2) on Attic. check_character resolves everything through
// the world service, so the real action runs without a server.
static akashi::World *buildCharacterWorld(akashi::RuleRegistry &f_registry, akashi::ServiceRegistry &f_services, QObject *f_parent)
{
    static QSettings s_blank_ini(QStringLiteral("tst_world_blank_areas.ini"), QSettings::IniFormat);
    auto *l_world = new akashi::World(&f_registry, &f_services, nullptr, &s_blank_ini, f_parent);
    f_services.registerService(std::shared_ptr<akashi::World>(l_world, [](auto *) {}));
    akashi::registerCoreRuleActions(nullptr, &f_registry);
    l_world->createFloor(QStringLiteral("Yard"));
    l_world->renameArea(0, QStringLiteral("Gate"));
    l_world->createArea(QStringLiteral("Annex"), 0);
    l_world->createFloor(QStringLiteral("Attic"));
    l_world->renameArea(2, QStringLiteral("Loft"));
    return l_world;
}

void tst_World::checkCharacterHoldsAClaimPerArea()
{
    akashi::RuleRegistry l_registry;
    akashi::ServiceRegistry l_services;
    akashi::World *l_world = buildCharacterWorld(l_registry, l_services, &l_services);
    l_world->areaById(0)->takeCharacter(2);

    const auto l_rule = l_registry.buildBefore(QStringLiteral("check_character"), l_services,
                                               {{QStringLiteral("policy"), QStringLiteral("unique_per_area")}});
    QVERIFY(l_rule.has_value());

    // Taken in the acting client's own area: blocked, and silently - the
    // default rule mirrors the mechanism's wordless refusal.
    const akashi::RuleVerdict l_verdict = (*l_rule)({.player_state_id = 4, .client_session_id = 4, .area_id = 0, .floor_id = 0, .payload = {{QStringLiteral("character_id"), 2}}, .services = &l_services});
    QVERIFY(!l_verdict.allowed);
    QVERIFY(l_verdict.reason.isEmpty());

    // Free next door, and a spectator switch claims nothing anywhere.
    QVERIFY((*l_rule)({.player_state_id = 4, .client_session_id = 4, .area_id = 1, .floor_id = 0, .payload = {{QStringLiteral("character_id"), 2}}, .services = &l_services}).allowed);
    QVERIFY((*l_rule)({.player_state_id = 4, .client_session_id = 4, .area_id = 0, .floor_id = 0, .payload = {{QStringLiteral("character_id"), -1}}, .services = &l_services}).allowed);
    QVERIFY((*l_rule)({.player_state_id = 4, .client_session_id = 4, .area_id = 0, .floor_id = 0, .payload = {}, .services = &l_services}).allowed);

    // A message argument gives the refusal a voice.
    const auto l_spoken = l_registry.buildBefore(QStringLiteral("check_character"), l_services,
                                                 {{QStringLiteral("policy"), QStringLiteral("unique_per_area")},
                                                  {QStringLiteral("message"), QStringLiteral("Someone is already playing that role.")}});
    QCOMPARE((*l_spoken)({.player_state_id = 4, .client_session_id = 4, .area_id = 0, .floor_id = 0, .payload = {{QStringLiteral("character_id"), 2}}, .services = &l_services}).reason,
             QString("Someone is already playing that role."));

    // The legacy player_joined shape keeps its spoken area default.
    const auto l_legacy = l_registry.buildBefore(QStringLiteral("check_character"), l_services, {});
    const akashi::RuleVerdict l_legacy_verdict = (*l_legacy)({.player_state_id = 4, .client_session_id = 4, .area_id = 0, .floor_id = 0, .payload = {{QStringLiteral("character_id"), 2}}, .services = &l_services});
    QVERIFY(!l_legacy_verdict.allowed);
    QCOMPARE(l_legacy_verdict.reason, QString("That character is already taken in Gate."));
}

void tst_World::checkCharacterHoldsAClaimPerFloor()
{
    akashi::RuleRegistry l_registry;
    akashi::ServiceRegistry l_services;
    akashi::World *l_world = buildCharacterWorld(l_registry, l_services, &l_services);
    l_world->areaById(0)->takeCharacter(2);

    const auto l_rule = l_registry.buildBefore(QStringLiteral("check_character"), l_services,
                                               {{QStringLiteral("policy"), QStringLiteral("unique_on_floor")},
                                                {QStringLiteral("message"), QStringLiteral("One Phoenix per floor.")}});
    QVERIFY(l_rule.has_value());

    // Taken anywhere on the floor blocks, even from the other area.
    const akashi::RuleVerdict l_verdict = (*l_rule)({.player_state_id = 4, .client_session_id = 4, .area_id = 1, .floor_id = 0, .payload = {{QStringLiteral("character_id"), 2}}, .services = &l_services});
    QVERIFY(!l_verdict.allowed);
    QCOMPARE(l_verdict.reason, QString("One Phoenix per floor."));

    // The claim does not reach the other floor, other characters stay
    // free, and without a message the floor-wide refusal is silent too.
    QVERIFY((*l_rule)({.player_state_id = 4, .client_session_id = 4, .area_id = 2, .floor_id = 1, .payload = {{QStringLiteral("character_id"), 2}}, .services = &l_services}).allowed);
    QVERIFY((*l_rule)({.player_state_id = 4, .client_session_id = 4, .area_id = 1, .floor_id = 0, .payload = {{QStringLiteral("character_id"), 3}}, .services = &l_services}).allowed);
    const auto l_silent = l_registry.buildBefore(QStringLiteral("check_character"), l_services,
                                                 {{QStringLiteral("policy"), QStringLiteral("unique_on_floor")}});
    const akashi::RuleVerdict l_silent_verdict = (*l_silent)({.player_state_id = 4, .client_session_id = 4, .area_id = 1, .floor_id = 0, .payload = {{QStringLiteral("character_id"), 2}}, .services = &l_services});
    QVERIFY(!l_silent_verdict.allowed);
    QVERIFY(l_silent_verdict.reason.isEmpty());
}

void tst_World::crossPhaseBuildsReturnNothingForEveryPairing()
{
    akashi::RuleRegistry l_registry;
    akashi::ServiceRegistry l_services;
    l_registry.registerBeforeAction(
        QStringLiteral("gate"),
        [](akashi::ServiceRegistry &, const QVariantMap &) -> akashi::BeforeRuleFunction {
            return [](const akashi::RuleContext &) { return akashi::RuleVerdict{}; };
        });
    l_registry.registerAfterAction(
        QStringLiteral("react"),
        [](akashi::ServiceRegistry &, const QVariantMap &) -> akashi::AfterRuleFunction {
            return [](const akashi::RuleContext &) {};
        });
    l_registry.registerTransformAction(
        QStringLiteral("rewrite"),
        [](akashi::ServiceRegistry &, const QVariantMap &) -> akashi::TransformRuleFunction {
            return [](const akashi::RuleContext &) { return QVariantMap{}; };
        });

    // The matched phase builds; every cross-pairing builds nothing.
    QVERIFY(l_registry.buildBefore(QStringLiteral("gate"), l_services, {}).has_value());
    QVERIFY(!l_registry.buildBefore(QStringLiteral("react"), l_services, {}).has_value());
    QVERIFY(!l_registry.buildBefore(QStringLiteral("rewrite"), l_services, {}).has_value());
    QVERIFY(!l_registry.buildAfter(QStringLiteral("gate"), l_services, {}).has_value());
    QVERIFY(l_registry.buildAfter(QStringLiteral("react"), l_services, {}).has_value());
    QVERIFY(!l_registry.buildAfter(QStringLiteral("rewrite"), l_services, {}).has_value());
    QVERIFY(!l_registry.buildTransform(QStringLiteral("gate"), l_services, {}).has_value());
    QVERIFY(!l_registry.buildTransform(QStringLiteral("react"), l_services, {}).has_value());
    QVERIFY(l_registry.buildTransform(QStringLiteral("rewrite"), l_services, {}).has_value());

    // A name nobody registered builds nothing in any phase.
    QVERIFY(!l_registry.buildBefore(QStringLiteral("missing"), l_services, {}).has_value());
    QVERIFY(!l_registry.buildAfter(QStringLiteral("missing"), l_services, {}).has_value());
    QVERIFY(!l_registry.buildTransform(QStringLiteral("missing"), l_services, {}).has_value());

    // actionPhase answers Before for names it never saw - hasAction is the
    // existence check, the phase alone proves nothing.
    QVERIFY(!l_registry.hasAction(QStringLiteral("missing")));
    QCOMPARE(l_registry.actionPhase(QStringLiteral("missing")), akashi::RulePhase::Before);
}

void tst_World::duplicateActionNamesAreRefused()
{
    akashi::RuleRegistry l_registry;
    akashi::ServiceRegistry l_services;

    const auto l_gate = [](const QString &f_reason) -> akashi::BeforeActionFactory {
        return [f_reason](akashi::ServiceRegistry &, const QVariantMap &) -> akashi::BeforeRuleFunction {
            return [f_reason](const akashi::RuleContext &) { return akashi::RuleVerdict{false, f_reason}; };
        };
    };
    l_registry.registerBeforeAction(QStringLiteral("gate"), l_gate(QStringLiteral("first owner")), QStringLiteral("plugin-a"));

    // Registering a taken name is refused with a warning naming the action
    // and both owners; the first owner keeps the definition and its unload
    // sweep still finds the name.
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("gate.*plugin-b.*plugin-a")));
    l_registry.registerBeforeAction(QStringLiteral("gate"), l_gate(QStringLiteral("second owner")), QStringLiteral("plugin-b"));

    const auto l_function = l_registry.buildBefore(QStringLiteral("gate"), l_services, {});
    QVERIFY(l_function.has_value());
    QCOMPARE((*l_function)({.player_state_id = 4, .client_session_id = 4, .area_id = 0, .floor_id = 0, .payload = {}, .services = nullptr}).reason, QString("first owner"));
    QCOMPARE(l_registry.actionsOwnedBy(QStringLiteral("plugin-a")), QStringList({QStringLiteral("gate")}));
    QVERIFY(l_registry.actionsOwnedBy(QStringLiteral("plugin-b")).isEmpty());

    // The refusal also holds across phases: the name keeps its phase.
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("gate.*plugin-c.*plugin-a")));
    l_registry.registerAfterAction(
        QStringLiteral("gate"),
        [](akashi::ServiceRegistry &, const QVariantMap &) -> akashi::AfterRuleFunction {
            return [](const akashi::RuleContext &) {};
        },
        QStringLiteral("plugin-c"));
    QCOMPARE(l_registry.actionPhase(QStringLiteral("gate")), akashi::RulePhase::Before);
    QVERIFY(l_registry.buildBefore(QStringLiteral("gate"), l_services, {}).has_value());
    QVERIFY(!l_registry.buildAfter(QStringLiteral("gate"), l_services, {}).has_value());
}

void tst_World::unknownOwnerSweepsAreHarmless()
{
    akashi::RuleRegistry l_registry;
    l_registry.registerBeforeAction(
        QStringLiteral("gate"),
        [](akashi::ServiceRegistry &, const QVariantMap &) -> akashi::BeforeRuleFunction {
            return [](const akashi::RuleContext &) { return akashi::RuleVerdict{}; };
        },
        QStringLiteral("plugin-a"));

    // Sweeping an owner that registered nothing removes nothing.
    l_registry.unregisterActions(QStringLiteral("plugin-x"));
    QVERIFY(l_registry.hasAction(QStringLiteral("gate")));

    akashi::Floor l_floor;
    l_floor.before_rules.append({akashi::AreaEvents::PlayerJoined, QStringLiteral("watch"),
                                 [](const akashi::RuleContext &) { return akashi::RuleVerdict{}; },
                                 QStringLiteral("plugin-a")});
    l_floor.after_rules.append({akashi::AreaEvents::PlayerLeft, QStringLiteral("react"),
                                [](const akashi::RuleContext &) {},
                                QStringLiteral("core")});
    l_floor.transform_rules.append({akashi::AreaEvents::IcMessageSent, QStringLiteral("rewrite"),
                                    [](const akashi::RuleContext &) { return QVariantMap{}; },
                                    QStringLiteral("config")});

    const int l_removed = akashi::RuleRegistry::removeRules(QStringLiteral("plugin-x"), {},
                                                            l_floor.before_rules, l_floor.after_rules, l_floor.transform_rules);
    QCOMPARE(l_removed, 0);
    QCOMPARE(l_floor.before_rules.size(), 1);
    QCOMPARE(l_floor.after_rules.size(), 1);
    QCOMPARE(l_floor.transform_rules.size(), 1);
}

void tst_World::dispatchSkipsEventsNothingMatches()
{
    bool l_before_ran = false;
    bool l_transform_ran = false;
    bool l_after_ran = false;

    QVector<akashi::BeforeRuleEntry> l_before = {
        {akashi::AreaEvents::IcMessageSent, QStringLiteral("block_all"),
         [&l_before_ran](const akashi::RuleContext &) {
             l_before_ran = true;
             return akashi::RuleVerdict{false, QStringLiteral("blocked")};
         },
         "test"},
    };
    QVector<akashi::TransformRuleEntry> l_transforms = {
        {akashi::AreaEvents::IcMessageSent, QStringLiteral("rewrite"),
         [&l_transform_ran](const akashi::RuleContext &) {
             l_transform_ran = true;
             return QVariantMap{{QStringLiteral("message"), QStringLiteral("rewritten")}};
         },
         "test"},
    };
    QVector<akashi::AfterRuleEntry> l_after = {
        {akashi::AreaEvents::IcMessageSent, QStringLiteral("react"),
         [&l_after_ran](const akashi::RuleContext &) { l_after_ran = true; },
         "test"},
    };

    // music_changed matches none of the ic_message_sent entries: the gate
    // answers the allowing default, the payload comes back untouched and
    // no reaction fires - on the area entries and the floor entries alike.
    const QVariantMap l_payload = {{QStringLiteral("message"), QStringLiteral("hello")}};
    const akashi::RuleContext l_context = {.player_state_id = 4, .client_session_id = 4, .area_id = 0, .floor_id = 0, .payload = l_payload, .services = nullptr};

    const akashi::RuleVerdict l_verdict = akashi::RuleRegistry::checkBefore(
        akashi::AreaEvents::MusicChanged, l_context, l_before, l_before);
    QVERIFY(l_verdict.allowed);
    QVERIFY(l_verdict.reason.isEmpty());
    QCOMPARE(akashi::RuleRegistry::runTransforms(akashi::AreaEvents::MusicChanged, l_context, l_transforms, l_transforms),
             l_payload);
    akashi::RuleRegistry::runAfter(akashi::AreaEvents::MusicChanged, l_context, l_after, l_after);

    QVERIFY(!l_before_ran);
    QVERIFY(!l_transform_ran);
    QVERIFY(!l_after_ran);
}

void tst_World::transformsMayIntroduceKeysThePayloadNeverHad()
{
    QVector<akashi::TransformRuleEntry> l_floor_rules = {
        {akashi::AreaEvents::IcMessageSent, QStringLiteral("sparkle"),
         [](const akashi::RuleContext &) {
             return QVariantMap{{QStringLiteral("glitter"), true}};
         },
         "test"},
    };

    const QVariantMap l_result = akashi::RuleRegistry::runTransforms(
        akashi::AreaEvents::IcMessageSent,
        {.player_state_id = 4, .client_session_id = 4, .area_id = 1, .floor_id = 0, .payload = {{QStringLiteral("message"), QStringLiteral("hi")}}, .services = nullptr},
        {}, l_floor_rules);

    // A key the payload never carried is merged in, not dropped. The verb
    // decides which keys it commits, so a stray key is inert downstream -
    // but later transforms in the chain do see it.
    QCOMPARE(l_result.value("glitter").toBool(), true);
    QCOMPARE(l_result.value("message").toString(), QString("hi"));
    QCOMPARE(l_result.size(), 2);
}

void tst_World::evidenceStoreIgnoresIndexesOutOfBounds()
{
    akashi::EvidenceStore l_store;
    l_store.append({"Sword", "A sword.", "sword.png"});
    l_store.append({"Photo", "A photo.", "photo.png"});

    l_store.remove(-1);
    l_store.remove(2);
    l_store.replace(-1, {"Fake", "Fake.", "fake.png"});
    l_store.replace(2, {"Fake", "Fake.", "fake.png"});
    l_store.swap(-1, 0);
    l_store.swap(0, 2);
    l_store.swap(-2, 5);
    l_store.revealToAll(-1);
    l_store.revealToAll(2);

    QCOMPARE(l_store.itemCount(), 2);
    QCOMPARE(l_store.items().at(0).name, QString("Sword"));
    QCOMPARE(l_store.items().at(0).description, QString("A sword."));
    QCOMPARE(l_store.items().at(1).name, QString("Photo"));
    QCOMPARE(l_store.items().at(1).description, QString("A photo."));
}

void tst_World::taggedDescriptionOnlyRewritesUntaggedHiddenItems()
{
    akashi::EvidenceStore l_store;

    // Outside hidden mode nothing is rewritten, whatever the access level.
    QCOMPARE(l_store.taggedDescription("Sharp."), QString("Sharp."));
    l_store.setAccess(akashi::EvidenceStore::Access::Mod);
    QCOMPARE(l_store.taggedDescription("Sharp."), QString("Sharp."));
    l_store.setAccess(akashi::EvidenceStore::Access::Cm);
    QCOMPARE(l_store.taggedDescription("Sharp."), QString("Sharp."));

    // Hidden mode tags the untagged once and never doubles an existing tag.
    l_store.setAccess(akashi::EvidenceStore::Access::HiddenCm);
    QCOMPARE(l_store.taggedDescription("Sharp."), QString("<owner=all>\nSharp."));
    QCOMPARE(l_store.taggedDescription("<owner=all>\nSharp."), QString("<owner=all>\nSharp."));
    QCOMPARE(l_store.taggedDescription("<owner=def>\nSharp."), QString("<owner=def>\nSharp."));
}

// Two floors with one area each and no registered rule actions: the
// smallest world the reshaping guards can be probed on.
static akashi::World *buildBareWorld(akashi::RuleRegistry &f_registry, akashi::ServiceRegistry &f_services, QObject *f_parent)
{
    static QSettings s_bare_ini(QStringLiteral("tst_world_bare_areas.ini"), QSettings::IniFormat);
    auto *l_world = new akashi::World(&f_registry, &f_services, nullptr, &s_bare_ini, f_parent);
    l_world->createFloor(QStringLiteral("Ground"));
    l_world->createFloor(QStringLiteral("Cellar"));
    return l_world;
}

void tst_World::createRefusesBlankNamesAndUnknownFloors()
{
    akashi::RuleRegistry l_registry;
    akashi::ServiceRegistry l_services;
    akashi::World *l_world = buildBareWorld(l_registry, l_services, &l_services);

    QCOMPARE(l_world->createArea(QStringLiteral("Annex"), 9), -1);
    QCOMPARE(l_world->createArea(QStringLiteral("Annex"), -1), -1);
    QCOMPARE(l_world->createArea(QStringLiteral("   "), 0), -1);
    // Floor names collide in any letter case.
    QCOMPARE(l_world->createFloor(QStringLiteral("ground")), -1);
    QCOMPARE(l_world->createFloor(QString()), -1);

    QCOMPARE(l_world->areaCount(), 2);
    QCOMPARE(l_world->floorCount(), 2);
}

void tst_World::renamesRefuseInvalidIdsAndNames()
{
    akashi::RuleRegistry l_registry;
    akashi::ServiceRegistry l_services;
    akashi::World *l_world = buildBareWorld(l_registry, l_services, &l_services);

    // Areas: unknown ids and blank names refuse without touching anything.
    QVERIFY(!l_world->renameArea(9, QStringLiteral("Vault")));
    QVERIFY(!l_world->renameArea(-1, QStringLiteral("Vault")));
    QVERIFY(!l_world->renameArea(0, QStringLiteral("   ")));
    QCOMPARE(l_world->areaName(0), QString("Unnamed Area"));

    // Floors add name collisions to that - including the floor's own
    // current name, in any letter case.
    QVERIFY(!l_world->renameFloor(9, QStringLiteral("Attic")));
    QVERIFY(!l_world->renameFloor(0, QString()));
    QVERIFY(!l_world->renameFloor(0, QStringLiteral("cellar")));
    QVERIFY(!l_world->renameFloor(0, QStringLiteral("Ground")));
    QCOMPARE(l_world->floorById(0)->name, QString("Ground"));

    // A fresh name still lands on both.
    QVERIFY(l_world->renameFloor(0, QStringLiteral("Lobby")));
    QCOMPARE(l_world->floorById(0)->name, QString("Lobby"));
    QVERIFY(l_world->renameArea(0, QStringLiteral("Vault")));
    QCOMPARE(l_world->areaName(0), QString("Vault"));
}

void tst_World::removeAreaRefusesItsGuards()
{
    akashi::RuleRegistry l_registry;
    akashi::ServiceRegistry l_services;
    akashi::World *l_world = buildBareWorld(l_registry, l_services, &l_services);
    QCOMPARE(l_world->areaCount(), 2);

    // An id that names no area refuses and maps nothing.
    QVector<int> l_mapping;
    auto l_refusal = l_world->removeArea(9, l_mapping);
    QVERIFY(l_refusal.has_value());
    QCOMPARE(*l_refusal, QString("There is no area with that ID."));
    QVERIFY(l_mapping.isEmpty());
    QCOMPARE(l_world->areaCount(), 2);

    // The floor's only area must stay; the floor is the removable unit.
    l_refusal = l_world->removeArea(0, l_mapping);
    QVERIFY(l_refusal.has_value());
    QCOMPARE(*l_refusal, QString("A floor needs at least one area; remove the floor instead."));
    QCOMPARE(l_world->areaCount(), 2);

    // An occupied area stays put too.
    QCOMPARE(l_world->createArea(QStringLiteral("Annex"), 0), 2);
    l_world->areaById(2)->addPlayer(7);
    l_refusal = l_world->removeArea(2, l_mapping);
    QVERIFY(l_refusal.has_value());
    QCOMPARE(*l_refusal, QString("That area is not empty."));
    QCOMPARE(l_world->areaCount(), 3);
    QVERIFY(l_mapping.isEmpty());

    // The player was the only obstacle.
    l_world->areaById(2)->removePlayer(7);
    QVERIFY(!l_world->removeArea(2, l_mapping).has_value());
    QCOMPARE(l_world->areaCount(), 2);
}

void tst_World::removeFloorRefusesItsGuards()
{
    akashi::RuleRegistry l_registry;
    akashi::ServiceRegistry l_services;
    akashi::World *l_world = buildBareWorld(l_registry, l_services, &l_services);

    QVector<int> l_mapping;
    auto l_refusal = l_world->removeFloor(9, l_mapping);
    QVERIFY(l_refusal.has_value());
    QCOMPARE(*l_refusal, QString("There is no floor with that ID."));

    // A floor with anyone on it stays.
    l_world->areaById(1)->addPlayer(4);
    l_refusal = l_world->removeFloor(1, l_mapping);
    QVERIFY(l_refusal.has_value());
    QCOMPARE(*l_refusal, QString("Not every area on that floor is empty."));
    QCOMPARE(l_world->floorCount(), 2);
    QVERIFY(l_mapping.isEmpty());

    // Emptied it goes - and the survivor then refuses as the last floor.
    l_world->areaById(1)->removePlayer(4);
    QVERIFY(!l_world->removeFloor(1, l_mapping).has_value());
    QCOMPARE(l_world->floorCount(), 1);
    l_refusal = l_world->removeFloor(0, l_mapping);
    QVERIFY(l_refusal.has_value());
    QCOMPARE(*l_refusal, QString("The last floor cannot be removed."));
    QCOMPARE(l_world->floorCount(), 1);
    QCOMPARE(l_world->areaCount(), 1);
}

static QString writeRulesFile(QTemporaryDir &f_dir, const QByteArray &f_json)
{
    const QString l_path = f_dir.filePath(QStringLiteral("areas.json"));
    // Fails the test here instead of returning a silent empty path.
    QFile l_file(l_path);
    if (!QTest::qVerify(l_file.open(QIODevice::WriteOnly | QIODevice::Text), "l_file.open(QIODevice::WriteOnly | QIODevice::Text)", qPrintable(l_path), __FILE__, __LINE__)) {
        return {};
    }
    l_file.write(f_json);
    return l_path;
}

void tst_World::configRulesSkipUnknownActionsAndWrongPhases()
{
    akashi::RuleRegistry l_registry;
    akashi::ServiceRegistry l_services;
    akashi::World *l_world = buildCharacterWorld(l_registry, l_services, &l_services);

    akashi::Floor *l_floor = l_world->floorById(0);
    const int l_before_count = l_floor->before_rules.size();
    const int l_after_count = l_floor->after_rules.size();
    const int l_transform_count = l_floor->transform_rules.size();

    QTemporaryDir l_dir;
    const QString l_path = writeRulesFile(l_dir, R"({
        "floors": {
            "Yard": {
                "rules": {
                    "ic_message_sent": {
                        "before": [
                            {"action": "does_not_exist"},
                            {"action": "send_message", "message": "an after action in the gate bucket"},
                            {"action": "check_setting", "setting": "music_allowed"}
                        ],
                        "transform": [{"action": "check_character"}],
                        "after": [{"action": "strip_shouts"}]
                    }
                }
            }
        }
    })");

    // The unknown name and the three phase mismatches are skipped loudly;
    // the one well-formed declaration still lands.
    for (int i = 0; i < 4; ++i) {
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("is not a registered action of that phase")));
    }
    l_world->applyConfigRules(l_path);

    QCOMPARE(l_floor->before_rules.size(), l_before_count + 1);
    QCOMPARE(l_floor->before_rules.last().action, QString("check_setting"));
    QCOMPARE(l_floor->before_rules.last().owner_id, QString("config"));
    QCOMPARE(l_floor->after_rules.size(), l_after_count);
    QCOMPARE(l_floor->transform_rules.size(), l_transform_count);
}

void tst_World::configRulesForUnknownScopesAttachNothing()
{
    akashi::RuleRegistry l_registry;
    akashi::ServiceRegistry l_services;
    akashi::World *l_world = buildCharacterWorld(l_registry, l_services, &l_services);

    const int l_yard_count = l_world->floorById(0)->before_rules.size();
    const int l_attic_count = l_world->floorById(1)->before_rules.size();

    QTemporaryDir l_dir;
    const QString l_path = writeRulesFile(l_dir, R"({
        "floors": {
            "Penthouse": {
                "rules": {
                    "ic_message_sent": {
                        "before": [{"action": "check_setting", "setting": "music_allowed"}]
                    }
                }
            }
        },
        "9:Nowhere": {
            "rules": {
                "ic_message_sent": {
                    "before": [{"action": "check_setting", "setting": "music_allowed"}]
                }
            }
        }
    })");

    // A floor name and an area index the world does not have both warn and
    // attach nowhere - especially not to some other floor or area.
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("unknown floor.*Penthouse")));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("unknown area index 9")));
    l_world->applyConfigRules(l_path);

    QCOMPARE(l_world->floorById(0)->before_rules.size(), l_yard_count);
    QCOMPARE(l_world->floorById(1)->before_rules.size(), l_attic_count);
    for (akashi::Area *l_area : l_world->areas()) {
        QVERIFY(l_area->beforeRules().isEmpty());
    }
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_World)

#include "tst_world.moc"
