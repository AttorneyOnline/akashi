// AI-generated: written by Claude.
#include "world/area.h"
#include "world/rule_registry.h"
#include "world/evidence_store.h"
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
    void areaRulesOverrideFloorRulesOfSameAction();
    void evidenceStoreNumbersItemsPerViewer();
    void beforeRulesGateWithoutTouchingAfterRules();
    void afterRulesActOnTheWorld();
    void pluginBeforeRuleObjectsRegisterAndReleaseOnUnload();
    void unregisteringAnOwnerRemovesItsRules();
    void pluginUnloadSweepTakesDirectAndActionBuiltRules();
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
    QVERIFY(l_area.removeOwner(4));
    QVERIFY(!l_area.hasOwners());
    QCOMPARE(l_area.lockState(), akashi::Area::LockState::Free);
    QVERIFY(!l_area.invited().contains(4));
    QVERIFY(!l_area.removeOwner(4));
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

    QCOMPARE(l_floor.character_policy, akashi::Floor::CharacterPolicy::UniquePerArea);
    QVERIFY(l_floor.area_ids.isEmpty());
    QVERIFY(l_floor.before_rules.isEmpty());
    QVERIFY(l_floor.after_rules.isEmpty());
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
    QVERIFY(!akashi::RuleRegistry::checkBefore(akashi::AreaEvents::IcMessageSent, {4, 2, 0, {}, nullptr},
                                               l_area2_rules, l_empty).allowed);
    // Area 3 has no rules — IC allowed.
    QVERIFY(akashi::RuleRegistry::checkBefore(akashi::AreaEvents::IcMessageSent, {4, 3, 0, {}, nullptr},
                                              l_empty, l_empty).allowed);
    // Floor 1 blocks evidence.
    QVERIFY(!akashi::RuleRegistry::checkBefore(akashi::AreaEvents::EvidencePresented, {4, 9, 1, {}, nullptr},
                                               l_empty, l_floor1_rules).allowed);
    // Floor 0 has no rules — evidence allowed.
    QVERIFY(akashi::RuleRegistry::checkBefore(akashi::AreaEvents::EvidencePresented, {4, 9, 0, {}, nullptr},
                                              l_empty, l_empty).allowed);
    // PlayerJoined has no rules anywhere — allowed.
    QVERIFY(akashi::RuleRegistry::checkBefore(akashi::AreaEvents::PlayerJoined, {4, 2, 0, {}, nullptr},
                                              l_area2_rules, l_empty).allowed);
}

void tst_World::firstBlockingRuleWins()
{
    QVector<akashi::BeforeRuleEntry> l_area_rules = {
        {akashi::AreaEvents::PlayerJoined, QStringLiteral("check_lock"),
         [](const akashi::RuleContext &) { return akashi::RuleVerdict{false, "the area says no"}; },
         "test"},
    };
    QVector<akashi::BeforeRuleEntry> l_floor_rules = {
        {akashi::AreaEvents::PlayerJoined, QStringLiteral("check_character"),
         [](const akashi::RuleContext &) { return akashi::RuleVerdict{false, "the floor says no"}; },
         "test"},
    };

    const auto l_verdict = akashi::RuleRegistry::checkBefore(akashi::AreaEvents::PlayerJoined, {4, 0, 0, {}, nullptr},
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
        {akashi::AreaEvents::EvidencePresented, QStringLiteral("check_permission"),
         [](const akashi::RuleContext &) { return akashi::RuleVerdict{}; },
         "test"},
    };
    QVector<akashi::BeforeRuleEntry> l_area_rules = {
        {akashi::AreaEvents::EvidencePresented, QStringLiteral("check_evidence"),
         [](const akashi::RuleContext &) { return akashi::RuleVerdict{}; },
         "test"},
    };

    // Area overrides check_evidence (allows), floor check_permission still runs (allows).
    QVERIFY(akashi::RuleRegistry::checkBefore(akashi::AreaEvents::EvidencePresented, {4, 2, 0, {}, nullptr},
                                              l_area_rules, l_floor_rules).allowed);
    // Without area rules, floor check_evidence blocks.
    QVector<akashi::BeforeRuleEntry> l_empty;
    QVERIFY(!akashi::RuleRegistry::checkBefore(akashi::AreaEvents::EvidencePresented, {4, 3, 0, {}, nullptr},
                                               l_empty, l_floor_rules).allowed);

    // Floor check_permission blocks, area only overrides check_evidence.
    QVector<akashi::BeforeRuleEntry> l_floor_rules2 = {
        {akashi::AreaEvents::EvidencePresented, QStringLiteral("check_evidence"),
         [](const akashi::RuleContext &) { return akashi::RuleVerdict{false, "floor blocks"}; },
         "test"},
        {akashi::AreaEvents::EvidencePresented, QStringLiteral("check_permission"),
         [](const akashi::RuleContext &) { return akashi::RuleVerdict{false, "no permission"}; },
         "test"},
    };

    const auto l_verdict = akashi::RuleRegistry::checkBefore(akashi::AreaEvents::EvidencePresented, {4, 2, 0, {}, nullptr},
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
        akashi::AreaEvents::PlayerJoined, {4, 0, 0, {}, nullptr},
        l_before, {});

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
                                   {4, 1, 0, {{QStringLiteral("message"), QStringLiteral("open sesame")}}, nullptr},
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
                                               {4, 0, 0, {{QStringLiteral("evidence_name"), QStringLiteral("Rusty Crowbar")}}, nullptr},
                                               l_area_rules, {}).allowed);
    QVERIFY(akashi::RuleRegistry::checkBefore(akashi::AreaEvents::EvidencePresented,
                                              {4, 0, 0, {{QStringLiteral("evidence_name"), QStringLiteral("Cursed Key")}}, nullptr},
                                              l_area_rules, {}).allowed);

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
    QCOMPARE(akashi::RuleRegistry::checkBefore(akashi::AreaEvents::PlayerJoined, {4, 0, 0, {}, nullptr},
                                               {}, l_floor.before_rules).reason,
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

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_World)

#include "tst_world.moc"
