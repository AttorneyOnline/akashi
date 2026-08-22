// AI-generated: written by Claude.
#include "core/permission_registry.h"

#include <QTest>

namespace tests {
namespace unittests {

class tst_PermissionRegistry : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void permissionRegistration();
    void permissionByCategory();
    void permissionUnregisterByOwner();
    void permissionsOwnedByIsSorted();
    void duplicatePermissionFails();

    void emptyPermissionIsTriviallyGranted();
    void resolveWithNoGrantsRefuses();
    void grantsMatchTheirAudience();
    void unknownPermissionGrantIsRefused();
    void restrictedPermissionNeverRidesAnEveryoneOffer();
    void restrictedPermissionStillReachesARole();
    void admitAnswersWithoutStoring();

    void scopedGrantsCoverOnlyTheirPlace();
    void duplicateGrantIsANoOp();
    void removeGrantTakesExactlyOne();
    void removeGrantsByOwnerSweeps();
    void grantsAreSortedDeterministically();

    void wornRoleHoldsThePermission();
    void roleAudienceMatchesAWornRole();
    void withoutAProviderOnlyTheLoginRoleIsWorn();

    void sourceAnswersItsInterestOnly();
    void sourceWithoutInterestsAnswersAnything();
    void duplicateSourceIdKeepsTheFirst();
    void sourceUnregisterByOwner();
    void sourceUnregisterUnknownOwnerIsANoOp();
    void setSourceInterestsReplacesTheSet();

    void sanctionMaskBeatsEveryContributor();
    void oneSanctionMayMaskSeveralActs();
    void sanctionMaskRegistrationValidates();

    void unregisteringPermissionsSweepsTheirGrants();
    void resolveExplainedOrdersDeterministically();
    void describeGrantNamesThePlace();
};

namespace {

// A registry with the handful of permissions these tests grant, so every
// case starts from the same catalog.
void seed(akashi::PermissionRegistry &f_registry)
{
    f_registry.registerPermission({QStringLiteral("ic_chat"), {}, {}, QStringLiteral("speech")});
    f_registry.registerPermission({QStringLiteral("ooc_chat"), {}, {}, QStringLiteral("speech")});
    f_registry.registerPermission({QStringLiteral("user"), {}, {}, QStringLiteral("lifecycle")});
    f_registry.registerPermission({QStringLiteral("enter"), {}, {}, QStringLiteral("area")});
    f_registry.registerPermission({QStringLiteral("kick"), {}, {}, QStringLiteral("moderation"), true});
}

akashi::PermissionQuery joined(const QString &f_permission)
{
    akashi::PermissionQuery l_query;
    l_query.permission = f_permission;
    l_query.is_joined = true;
    return l_query;
}

} // namespace

void tst_PermissionRegistry::permissionRegistration()
{
    akashi::PermissionRegistry l_registry;
    akashi::PermissionInfo l_info{QStringLiteral("kick"), QStringLiteral("Kick"), {}, QStringLiteral("moderation"), true};
    QVERIFY(l_registry.registerPermission(l_info));
    QVERIFY(l_registry.isRegistered("kick"));

    if (auto l_found = l_registry.permissionById("kick")) {
        QCOMPARE(l_found->display_name, QStringLiteral("Kick"));
        QVERIFY(l_found->restricted);
    }
    else {
        QFAIL("permission not found");
    }
}

void tst_PermissionRegistry::permissionByCategory()
{
    akashi::PermissionRegistry l_registry;
    l_registry.registerPermission({QStringLiteral("kick"), {}, {}, QStringLiteral("moderation")});
    l_registry.registerPermission({QStringLiteral("ban"), {}, {}, QStringLiteral("moderation")});
    l_registry.registerPermission({QStringLiteral("motd"), {}, {}, QStringLiteral("admin")});

    QCOMPARE(l_registry.permissionsByCategory("moderation").size(), 2);
    QCOMPARE(l_registry.permissionsByCategory("admin").size(), 1);
}

void tst_PermissionRegistry::permissionUnregisterByOwner()
{
    akashi::PermissionRegistry l_registry;
    l_registry.registerPermission({QStringLiteral("custom.perm"), {}, {}, {}}, QStringLiteral("plugin_x"));
    QVERIFY(l_registry.isRegistered("custom.perm"));
    const QStringList l_dropped = l_registry.unregisterAllPermissions(QStringLiteral("plugin_x"));
    QCOMPARE(l_dropped, QStringList{QStringLiteral("custom.perm")});
    QVERIFY(!l_registry.isRegistered("custom.perm"));
}

void tst_PermissionRegistry::permissionsOwnedByIsSorted()
{
    akashi::PermissionRegistry l_registry;
    l_registry.registerPermission({QStringLiteral("zeta"), {}, {}, {}}, QStringLiteral("plugin_x"));
    l_registry.registerPermission({QStringLiteral("alpha"), {}, {}, {}}, QStringLiteral("plugin_x"));
    l_registry.registerPermission({QStringLiteral("other"), {}, {}, {}}, QStringLiteral("plugin_y"));

    QCOMPARE(l_registry.permissionsOwnedBy(QStringLiteral("plugin_x")),
             (QStringList{QStringLiteral("alpha"), QStringLiteral("zeta")}));
}

void tst_PermissionRegistry::duplicatePermissionFails()
{
    akashi::PermissionRegistry l_registry;
    l_registry.registerPermission({QStringLiteral("kick"), {}, {}, {}});
    QVERIFY(!l_registry.registerPermission({QStringLiteral("kick"), {}, {}, {}}));
}

void tst_PermissionRegistry::emptyPermissionIsTriviallyGranted()
{
    akashi::PermissionRegistry l_registry;
    akashi::PermissionQuery l_query;
    QVERIFY(l_registry.resolve(l_query));
    l_query.permission = QStringLiteral("none");
    QVERIFY(l_registry.resolve(l_query));
}

void tst_PermissionRegistry::resolveWithNoGrantsRefuses()
{
    akashi::PermissionRegistry l_registry;
    seed(l_registry);
    QVERIFY(!l_registry.resolve(joined(QStringLiteral("ic_chat"))));
}

void tst_PermissionRegistry::grantsMatchTheirAudience()
{
    akashi::PermissionRegistry l_registry;
    seed(l_registry);
    QVERIFY(l_registry.addGrant({QStringLiteral("user"), akashi::Audience::everyone(), akashi::GrantScope::Server, -1, QStringLiteral("core")}));
    QVERIFY(l_registry.addGrant({QStringLiteral("kick"), akashi::Audience::role(QStringLiteral("MOD")), akashi::GrantScope::Server, -1, QStringLiteral("roles")}));
    QVERIFY(l_registry.addGrant({QStringLiteral("enter"), akashi::Audience::person(QStringLiteral("ipid-1")), akashi::GrantScope::Server, -1, QStringLiteral("core")}));

    // Everyone covers joined sessions and nobody else.
    akashi::PermissionQuery l_query;
    l_query.permission = QStringLiteral("user");
    QVERIFY(!l_registry.resolve(l_query));
    l_query.is_joined = true;
    QVERIFY(l_registry.resolve(l_query));

    // A role audience wants the role worn.
    l_query = joined(QStringLiteral("kick"));
    QVERIFY(!l_registry.resolve(l_query));
    l_query.is_authenticated = true;
    l_query.acl_role_id = QStringLiteral("MOD");
    QVERIFY(l_registry.resolve(l_query));
    l_query.acl_role_id = QStringLiteral("HELPER");
    QVERIFY(!l_registry.resolve(l_query));

    // A person audience wants that IPID, and an empty one never matches.
    l_query = joined(QStringLiteral("enter"));
    QVERIFY(!l_registry.resolve(l_query));
    l_query.ipid = QStringLiteral("ipid-1");
    QVERIFY(l_registry.resolve(l_query));
}

void tst_PermissionRegistry::unknownPermissionGrantIsRefused()
{
    akashi::PermissionRegistry l_registry;
    seed(l_registry);
    QVERIFY(!l_registry.addGrant({QStringLiteral("nosuchperm"), akashi::Audience::everyone(), akashi::GrantScope::Server, -1, QStringLiteral("config")}));
}

void tst_PermissionRegistry::restrictedPermissionNeverRidesAnEveryoneOffer()
{
    akashi::PermissionRegistry l_registry;
    seed(l_registry);
    // kick is restricted, so no everyone-shaped audience may carry it -
    // at any scope, from any door.
    QVERIFY(!l_registry.addGrant({QStringLiteral("kick"), akashi::Audience::everyone(), akashi::GrantScope::Server, -1, QStringLiteral("config")}));
    QVERIFY(!l_registry.addGrant({QStringLiteral("kick"), akashi::Audience::everyone(), akashi::GrantScope::Area, 3, QStringLiteral("config")}));
    QVERIFY(!l_registry.addGrant({QStringLiteral("kick"), akashi::Audience::participants(), akashi::GrantScope::Area, 3, QStringLiteral("plugin_x")}));
    QVERIFY(l_registry.grants().isEmpty());
}

void tst_PermissionRegistry::restrictedPermissionStillReachesARole()
{
    akashi::PermissionRegistry l_registry;
    seed(l_registry);
    QVERIFY(l_registry.addGrant({QStringLiteral("kick"), akashi::Audience::role(QStringLiteral("MOD")), akashi::GrantScope::Server, -1, QStringLiteral("roles")}));
    QVERIFY(l_registry.addGrant({QStringLiteral("kick"), akashi::Audience::person(QStringLiteral("ipid-1")), akashi::GrantScope::Server, -1, QStringLiteral("command")}));
    QCOMPARE(l_registry.grants().size(), 2);
}

void tst_PermissionRegistry::admitAnswersWithoutStoring()
{
    akashi::PermissionRegistry l_registry;
    seed(l_registry);
    const akashi::Grant l_good{QStringLiteral("ic_chat"), akashi::Audience::everyone(), akashi::GrantScope::Area, 2, QStringLiteral("config")};
    QVERIFY(l_registry.admit(l_good, QStringLiteral("area Courtroom")).has_value());
    QVERIFY(!l_registry.admit({QStringLiteral("kick"), akashi::Audience::everyone(), akashi::GrantScope::Server, -1, QStringLiteral("config")}, QStringLiteral("the everyone section")).has_value());
    // Asking is not storing.
    QVERIFY(l_registry.grants().isEmpty());
}

void tst_PermissionRegistry::scopedGrantsCoverOnlyTheirPlace()
{
    akashi::PermissionRegistry l_registry;
    seed(l_registry);
    l_registry.addGrant({QStringLiteral("ic_chat"), akashi::Audience::everyone(), akashi::GrantScope::Area, 4, QStringLiteral("config")});
    l_registry.addGrant({QStringLiteral("ooc_chat"), akashi::Audience::everyone(), akashi::GrantScope::Floor, 1, QStringLiteral("config")});

    akashi::PermissionQuery l_query = joined(QStringLiteral("ic_chat"));
    l_query.area_id = 4;
    l_query.floor_id = 0;
    QVERIFY(l_registry.resolve(l_query));
    l_query.area_id = 5;
    QVERIFY(!l_registry.resolve(l_query));

    l_query = joined(QStringLiteral("ooc_chat"));
    l_query.area_id = 9;
    l_query.floor_id = 1;
    QVERIFY(l_registry.resolve(l_query));
    l_query.floor_id = 2;
    QVERIFY(!l_registry.resolve(l_query));
}

void tst_PermissionRegistry::duplicateGrantIsANoOp()
{
    akashi::PermissionRegistry l_registry;
    seed(l_registry);
    const akashi::Grant l_grant{QStringLiteral("ic_chat"), akashi::Audience::everyone(), akashi::GrantScope::Server, -1, QStringLiteral("baseline")};
    QVERIFY(l_registry.addGrant(l_grant));
    QVERIFY(l_registry.addGrant(l_grant));
    QCOMPARE(l_registry.grants().size(), 1);
}

void tst_PermissionRegistry::removeGrantTakesExactlyOne()
{
    akashi::PermissionRegistry l_registry;
    seed(l_registry);
    const akashi::Grant l_area{QStringLiteral("ic_chat"), akashi::Audience::everyone(), akashi::GrantScope::Area, 4, QStringLiteral("config")};
    const akashi::Grant l_server{QStringLiteral("ic_chat"), akashi::Audience::everyone(), akashi::GrantScope::Server, -1, QStringLiteral("baseline")};
    l_registry.addGrant(l_area);
    l_registry.addGrant(l_server);

    QVERIFY(l_registry.removeGrant(l_area));
    QCOMPARE(l_registry.grants().size(), 1);
    // The place is part of the identity, so removing it twice fails.
    QVERIFY(!l_registry.removeGrant(l_area));
    QVERIFY(l_registry.removeGrant(l_server));
    QVERIFY(l_registry.grants().isEmpty());
}

void tst_PermissionRegistry::removeGrantsByOwnerSweeps()
{
    akashi::PermissionRegistry l_registry;
    seed(l_registry);
    l_registry.addGrant({QStringLiteral("ic_chat"), akashi::Audience::everyone(), akashi::GrantScope::Area, 4, QStringLiteral("config")});
    l_registry.addGrant({QStringLiteral("ooc_chat"), akashi::Audience::everyone(), akashi::GrantScope::Floor, 1, QStringLiteral("config")});
    l_registry.addGrant({QStringLiteral("user"), akashi::Audience::everyone(), akashi::GrantScope::Server, -1, QStringLiteral("core")});

    l_registry.removeGrantsByOwner(QStringLiteral("config"));
    QCOMPARE(l_registry.grants().size(), 1);
    QCOMPARE(l_registry.grants().first().owner, QStringLiteral("core"));
}

void tst_PermissionRegistry::grantsAreSortedDeterministically()
{
    akashi::PermissionRegistry l_registry;
    seed(l_registry);
    l_registry.addGrant({QStringLiteral("ooc_chat"), akashi::Audience::everyone(), akashi::GrantScope::Area, 7, QStringLiteral("config")});
    l_registry.addGrant({QStringLiteral("ic_chat"), akashi::Audience::everyone(), akashi::GrantScope::Area, 4, QStringLiteral("config")});
    l_registry.addGrant({QStringLiteral("ic_chat"), akashi::Audience::everyone(), akashi::GrantScope::Server, -1, QStringLiteral("baseline")});

    const QList<akashi::Grant> l_grants = l_registry.grants();
    QCOMPARE(l_grants.size(), 3);
    // Permission first, then the widest scope inward.
    QCOMPARE(l_grants.at(0).permission, QStringLiteral("ic_chat"));
    QCOMPARE(l_grants.at(0).scope, akashi::GrantScope::Server);
    QCOMPARE(l_grants.at(1).permission, QStringLiteral("ic_chat"));
    QCOMPARE(l_grants.at(1).scope, akashi::GrantScope::Area);
    QCOMPARE(l_grants.at(2).permission, QStringLiteral("ooc_chat"));
}

void tst_PermissionRegistry::wornRoleHoldsThePermission()
{
    akashi::PermissionRegistry l_registry;
    seed(l_registry);
    // Owning area 4 is wearing the case-manager keyring in it - the
    // contextual role, conferred by live state rather than by login.
    l_registry.setRoleProvider([](const akashi::PermissionQuery &f_query) -> QStringList {
        return f_query.area_id == 4 ? QStringList{QStringLiteral("@cm")} : QStringList{};
    });
    l_registry.setRoleHolds([](const QString &f_role, const QString &f_permission) {
        return f_role == QStringLiteral("@cm") && f_permission == QStringLiteral("enter");
    });

    akashi::PermissionQuery l_query = joined(QStringLiteral("enter"));
    l_query.area_id = 4;
    QVERIFY(l_registry.resolve(l_query));
    // Walk next door and the keyring comes off.
    l_query.area_id = 5;
    QVERIFY(!l_registry.resolve(l_query));
    // It only holds what it holds.
    l_query.permission = QStringLiteral("kick");
    l_query.area_id = 4;
    QVERIFY(!l_registry.resolve(l_query));
}

void tst_PermissionRegistry::roleAudienceMatchesAWornRole()
{
    akashi::PermissionRegistry l_registry;
    seed(l_registry);
    l_registry.addGrant({QStringLiteral("ic_chat"), akashi::Audience::role(QStringLiteral("@cm")), akashi::GrantScope::Server, -1, QStringLiteral("config")});
    l_registry.setRoleProvider([](const akashi::PermissionQuery &f_query) -> QStringList {
        return f_query.area_id == 4 ? QStringList{QStringLiteral("@cm")} : QStringList{};
    });
    // No roleHolds installed: the keyring holds nothing of its own, so
    // the grant is the only way through - a worn role satisfies a
    // role-audience offer.
    akashi::PermissionQuery l_query = joined(QStringLiteral("ic_chat"));
    l_query.area_id = 4;
    QVERIFY(l_registry.resolve(l_query));
    l_query.area_id = 5;
    QVERIFY(!l_registry.resolve(l_query));
}

void tst_PermissionRegistry::withoutAProviderOnlyTheLoginRoleIsWorn()
{
    akashi::PermissionRegistry l_registry;
    seed(l_registry);
    akashi::PermissionQuery l_query = joined(QStringLiteral("kick"));
    QVERIFY(l_registry.rolesWorn(l_query).isEmpty());
    l_query.is_authenticated = true;
    l_query.acl_role_id = QStringLiteral("MOD");
    QCOMPARE(l_registry.rolesWorn(l_query), QStringList{QStringLiteral("MOD")});
}

void tst_PermissionRegistry::sourceAnswersItsInterestOnly()
{
    akashi::PermissionRegistry l_registry;
    seed(l_registry);
    int l_calls = 0;
    l_registry.registerGrantSource(
        QStringLiteral("lock_state"), QStringLiteral("the area's lock"),
        [&l_calls](const akashi::PermissionQuery &) {
            l_calls++;
            return true;
        },
        QStringLiteral("core"), {QStringLiteral("enter")});

    // A query it declared no interest in never reaches it, so a source
    // cannot answer a question it was not asked.
    QVERIFY(!l_registry.resolve(joined(QStringLiteral("ic_chat"))));
    QCOMPARE(l_calls, 0);
    QVERIFY(l_registry.resolve(joined(QStringLiteral("enter"))));
    QCOMPARE(l_calls, 1);
}

void tst_PermissionRegistry::sourceWithoutInterestsAnswersAnything()
{
    akashi::PermissionRegistry l_registry;
    seed(l_registry);
    l_registry.registerGrantSource(
        QStringLiteral("blanket"), QStringLiteral("a plugin says so"),
        [](const akashi::PermissionQuery &f_query) { return f_query.ipid == QStringLiteral("vip"); },
        QStringLiteral("plugin_x"));

    akashi::PermissionQuery l_query = joined(QStringLiteral("ic_chat"));
    QVERIFY(!l_registry.resolve(l_query));
    l_query.ipid = QStringLiteral("vip");
    QVERIFY(l_registry.resolve(l_query));
}

void tst_PermissionRegistry::duplicateSourceIdKeepsTheFirst()
{
    akashi::PermissionRegistry l_registry;
    seed(l_registry);
    QVERIFY(l_registry.registerGrantSource(
        QStringLiteral("gate"), QStringLiteral("the first"),
        [](const akashi::PermissionQuery &) { return true; }, QStringLiteral("core")));
    QVERIFY(!l_registry.registerGrantSource(
        QStringLiteral("gate"), QStringLiteral("the second"),
        [](const akashi::PermissionQuery &) { return false; }, QStringLiteral("plugin_x")));
    QVERIFY(l_registry.resolve(joined(QStringLiteral("ic_chat"))));
}

void tst_PermissionRegistry::sourceUnregisterByOwner()
{
    akashi::PermissionRegistry l_registry;
    seed(l_registry);
    l_registry.registerGrantSource(
        QStringLiteral("plugin_source"), QStringLiteral("a plugin says so"),
        [](const akashi::PermissionQuery &) { return true; }, QStringLiteral("plugin_x"));
    QVERIFY(l_registry.resolve(joined(QStringLiteral("ic_chat"))));

    l_registry.unregisterAllGrantSources(QStringLiteral("plugin_x"));
    QVERIFY(!l_registry.resolve(joined(QStringLiteral("ic_chat"))));
}

void tst_PermissionRegistry::sourceUnregisterUnknownOwnerIsANoOp()
{
    akashi::PermissionRegistry l_registry;
    seed(l_registry);
    l_registry.registerGrantSource(
        QStringLiteral("granter"), QStringLiteral("a plugin says so"),
        [](const akashi::PermissionQuery &) { return true; }, QStringLiteral("plugin_x"));
    l_registry.unregisterAllGrantSources(QStringLiteral("plugin_y"));
    QVERIFY(l_registry.resolve(joined(QStringLiteral("ic_chat"))));
}

void tst_PermissionRegistry::setSourceInterestsReplacesTheSet()
{
    akashi::PermissionRegistry l_registry;
    seed(l_registry);
    l_registry.registerGrantSource(
        QStringLiteral("keyring"), QStringLiteral("the keyring"),
        [](const akashi::PermissionQuery &) { return true; },
        QStringLiteral("core"), {QStringLiteral("enter")});

    QVERIFY(!l_registry.resolve(joined(QStringLiteral("ic_chat"))));
    // The case-manager bundle moves when the roles file reloads.
    QVERIFY(l_registry.setSourceInterests(QStringLiteral("keyring"), {QStringLiteral("ic_chat")}));
    QVERIFY(l_registry.resolve(joined(QStringLiteral("ic_chat"))));
    QVERIFY(!l_registry.resolve(joined(QStringLiteral("enter"))));
    QVERIFY(!l_registry.setSourceInterests(QStringLiteral("nosuchsource"), {}));
}

void tst_PermissionRegistry::sanctionMaskBeatsEveryContributor()
{
    akashi::PermissionRegistry l_registry;
    seed(l_registry);
    l_registry.addGrant({QStringLiteral("ic_chat"), akashi::Audience::everyone(), akashi::GrantScope::Server, -1, QStringLiteral("core")});
    l_registry.setRoleProvider([](const akashi::PermissionQuery &) -> QStringList { return {QStringLiteral("SUPER")}; });
    l_registry.setRoleHolds([](const QString &, const QString &) { return true; });
    QVERIFY(l_registry.registerSanctionMask(QStringLiteral("muted"), QStringLiteral("ic_chat")));

    akashi::PermissionQuery l_query = joined(QStringLiteral("ic_chat"));
    QVERIFY(l_registry.resolve(l_query));

    // Even a role that holds everything loses the masked act.
    l_query.sanctions.insert(QStringLiteral("muted"));
    QVERIFY(!l_registry.resolve(l_query));
    const akashi::Resolution l_explained = l_registry.resolveExplained(l_query);
    QVERIFY(!l_explained.allowed);
    QCOMPARE(l_explained.masked_by, QStringList{QStringLiteral("muted")});

    // Another sanction leaves it alone.
    l_query.sanctions.clear();
    l_query.sanctions.insert(QStringLiteral("gimped"));
    QVERIFY(l_registry.resolve(l_query));
}

void tst_PermissionRegistry::oneSanctionMayMaskSeveralActs()
{
    akashi::PermissionRegistry l_registry;
    seed(l_registry);
    l_registry.addGrant({QStringLiteral("ic_chat"), akashi::Audience::everyone(), akashi::GrantScope::Server, -1, QStringLiteral("core")});
    l_registry.addGrant({QStringLiteral("ooc_chat"), akashi::Audience::everyone(), akashi::GrantScope::Server, -1, QStringLiteral("core")});
    QVERIFY(l_registry.registerSanctionMask(QStringLiteral("silenced"), QStringLiteral("ic_chat")));
    QVERIFY(l_registry.registerSanctionMask(QStringLiteral("silenced"), QStringLiteral("ooc_chat")));

    akashi::PermissionQuery l_query = joined(QStringLiteral("ic_chat"));
    l_query.sanctions.insert(QStringLiteral("silenced"));
    QVERIFY(!l_registry.resolve(l_query));
    l_query.permission = QStringLiteral("ooc_chat");
    QVERIFY(!l_registry.resolve(l_query));
}

void tst_PermissionRegistry::sanctionMaskRegistrationValidates()
{
    akashi::PermissionRegistry l_registry;
    seed(l_registry);
    // An unknown permission cannot be masked.
    QVERIFY(!l_registry.registerSanctionMask(QStringLiteral("muted"), QStringLiteral("nosuchperm")));
    QVERIFY(l_registry.registerSanctionMask(QStringLiteral("muted"), QStringLiteral("ic_chat")));
    // The same pair twice is the refusal - two owners would each think
    // they held it.
    QVERIFY(!l_registry.registerSanctionMask(QStringLiteral("muted"), QStringLiteral("ic_chat")));

    l_registry.unregisterAllSanctionMasks(QString());
    akashi::PermissionQuery l_query = joined(QStringLiteral("ic_chat"));
    l_query.sanctions.insert(QStringLiteral("muted"));
    l_registry.addGrant({QStringLiteral("ic_chat"), akashi::Audience::everyone(), akashi::GrantScope::Server, -1, QStringLiteral("core")});
    QVERIFY(l_registry.resolve(l_query));
}

void tst_PermissionRegistry::unregisteringPermissionsSweepsTheirGrants()
{
    akashi::PermissionRegistry l_registry;
    seed(l_registry);
    l_registry.registerPermission({QStringLiteral("plugin.act"), {}, {}, {}}, QStringLiteral("plugin_x"));
    // An area offer of the plugin's act, exactly as areas.json would
    // compile one.
    l_registry.addGrant({QStringLiteral("plugin.act"), akashi::Audience::everyone(), akashi::GrantScope::Area, 4, QStringLiteral("config")});

    akashi::PermissionQuery l_query = joined(QStringLiteral("plugin.act"));
    l_query.area_id = 4;
    QVERIFY(l_registry.resolve(l_query));

    // Unloading the plugin takes the catalog entry and the offer that
    // leaned on it together - an offer must never outlive its permission.
    l_registry.unregisterAllPermissions(QStringLiteral("plugin_x"));
    QVERIFY(!l_registry.resolve(l_query));
    QVERIFY(l_registry.grants().isEmpty());
}

void tst_PermissionRegistry::resolveExplainedOrdersDeterministically()
{
    akashi::PermissionRegistry l_registry;
    seed(l_registry);
    l_registry.setRoleProvider([](const akashi::PermissionQuery &) -> QStringList { return {QStringLiteral("MOD")}; });
    l_registry.setRoleHolds([](const QString &, const QString &) { return true; });
    l_registry.addGrant({QStringLiteral("ic_chat"), akashi::Audience::person(QStringLiteral("ipid-1")), akashi::GrantScope::Server, -1, QStringLiteral("command")});
    l_registry.addGrant({QStringLiteral("ic_chat"), akashi::Audience::everyone(), akashi::GrantScope::Area, 4, QStringLiteral("config")});
    l_registry.addGrant({QStringLiteral("ic_chat"), akashi::Audience::everyone(), akashi::GrantScope::Server, -1, QStringLiteral("baseline")});
    l_registry.registerGrantSource(
        QStringLiteral("live"), QStringLiteral("a live fact"),
        [](const akashi::PermissionQuery &) { return true; }, QStringLiteral("core"));

    akashi::PermissionQuery l_query = joined(QStringLiteral("ic_chat"));
    l_query.area_id = 4;
    l_query.ipid = QStringLiteral("ipid-1");
    const akashi::Resolution l_explained = l_registry.resolveExplained(l_query);

    QVERIFY(l_explained.allowed);
    QCOMPARE(l_explained.contributions.size(), 5);
    // Roles worn, then offers from the widest scope inward with the
    // personal one last, then the live facts.
    QCOMPARE(l_explained.contributions.at(0).kind, akashi::Contribution::Kind::Role);
    QCOMPARE(l_explained.contributions.at(1).grant->scope, akashi::GrantScope::Server);
    QCOMPARE(l_explained.contributions.at(2).grant->scope, akashi::GrantScope::Area);
    QCOMPARE(l_explained.contributions.at(3).grant->audience.kind, akashi::AudienceKind::Person);
    QCOMPARE(l_explained.contributions.at(4).kind, akashi::Contribution::Kind::Source);
    QCOMPARE(l_explained.contributions.at(4).because, QStringLiteral("a live fact"));
}

void tst_PermissionRegistry::describeGrantNamesThePlace()
{
    const akashi::Grant l_server{QStringLiteral("ic_chat"), akashi::Audience::everyone(), akashi::GrantScope::Server, -1, QStringLiteral("baseline")};
    QCOMPARE(akashi::describeGrant(l_server), QStringLiteral("ic_chat -> everyone @ server [baseline]"));

    const akashi::Grant l_area{QStringLiteral("music.jukebox"), akashi::Audience::role(QStringLiteral("MOD")), akashi::GrantScope::Area, 3, QStringLiteral("config")};
    QCOMPARE(akashi::describeGrant(l_area, QStringLiteral("Courtroom")),
             QStringLiteral("music.jukebox -> role MOD @ area Courtroom [config]"));
    // Without a name to hand, the id prints.
    QCOMPARE(akashi::describeGrant(l_area), QStringLiteral("music.jukebox -> role MOD @ area 3 [config]"));
}

} // namespace unittests
} // namespace tests

QTEST_MAIN(tests::unittests::tst_PermissionRegistry)
#include "tst_permission_registry.moc"
