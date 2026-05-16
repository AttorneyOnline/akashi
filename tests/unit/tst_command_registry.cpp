// AI-generated: written by Claude.
#include "core/command_context.h"
#include "core/command_registry.h"
#include "core/command_spec.h"
#include "core/permission_registry.h"
#include "acl_roles_handler.h"

#include <QTemporaryDir>
#include <QTest>

namespace tests {
namespace unittests {

class tst_CommandRegistry : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void registerAndLookup();
    void lookupByAlias();
    void caseInsensitiveLookup();
    void duplicateRegistrationFails();
    void unregisterByOwner();
    void containsCheck();
    void commandNamesList();

    void permissionRegistration();
    void permissionByCategory();
    void permissionUnregisterByOwner();
    void duplicatePermissionFails();

    void resolverChainPriorityOrder();
    void resolverGrantedShortCircuits();
    void resolverDeniedShortCircuits();
    void resolverAllNoOpinionDenies();
    void resolverUnregisterByOwner();
    void pluginResolverAtCustomPriority();

    void aclRoleStringCanPerform();
    void aclRoleSuperWildcard();
    void aclRoleEmptyIsNone();

    void applyExtensionsAddsAliases();
    void applyExtensionsOverridesPermissions();
    void applyExtensionsSkipsUnknownCommand();
    void applyExtensionsSkipsCollidingAlias();
    void applyExtensionsMissingFileIsNoop();
    void applyExtensionsSilentlySkipsSameCommandAlias();
};

void tst_CommandRegistry::registerAndLookup()
{
    akashi::CommandRegistry l_registry;
    bool l_handler_called = false;
    akashi::CommandSpec l_spec{QStringLiteral("test"), {}, {}, 0, {}, {}};
    QVERIFY(l_registry.registerCommand(l_spec, [&](akashi::CommandContext &) { l_handler_called = true; }));
    QVERIFY(l_registry.contains("test"));

    if (auto l_found = l_registry.spec("test")) {
        QCOMPARE(l_found->name, QStringLiteral("test"));
    }
    else {
        QFAIL("spec not found");
    }

    auto l_handler = l_registry.handler("test");
    QVERIFY(l_handler);
}

void tst_CommandRegistry::lookupByAlias()
{
    akashi::CommandRegistry l_registry;
    akashi::CommandSpec l_spec{QStringLiteral("getarea"), {QStringLiteral("ga")}, {}, 0, {}, {}};
    QVERIFY(l_registry.registerCommand(l_spec, [](akashi::CommandContext &) {}));

    QVERIFY(l_registry.contains("ga"));
    if (auto l_found = l_registry.spec("ga")) {
        QCOMPARE(l_found->name, QStringLiteral("getarea"));
    }
    else {
        QFAIL("alias lookup failed");
    }
}

void tst_CommandRegistry::caseInsensitiveLookup()
{
    akashi::CommandRegistry l_registry;
    akashi::CommandSpec l_spec{QStringLiteral("GetArea"), {}, {}, 0, {}, {}};
    QVERIFY(l_registry.registerCommand(l_spec, [](akashi::CommandContext &) {}));
    QVERIFY(l_registry.contains("getarea"));
    QVERIFY(l_registry.contains("GETAREA"));
}

void tst_CommandRegistry::duplicateRegistrationFails()
{
    akashi::CommandRegistry l_registry;
    akashi::CommandSpec l_spec{QStringLiteral("test"), {}, {}, 0, {}, {}};
    QVERIFY(l_registry.registerCommand(l_spec, [](akashi::CommandContext &) {}));
    QVERIFY(!l_registry.registerCommand(l_spec, [](akashi::CommandContext &) {}));
}

void tst_CommandRegistry::unregisterByOwner()
{
    akashi::CommandRegistry l_registry;
    akashi::CommandSpec l_spec{QStringLiteral("test"), {QStringLiteral("t")}, {}, 0, {}, {}};
    QVERIFY(l_registry.registerCommand(l_spec, [](akashi::CommandContext &) {}, QStringLiteral("plugin_a")));
    QVERIFY(l_registry.contains("test"));
    QVERIFY(l_registry.contains("t"));

    l_registry.unregisterAll(QStringLiteral("plugin_a"));
    QVERIFY(!l_registry.contains("test"));
    QVERIFY(!l_registry.contains("t"));
}

void tst_CommandRegistry::containsCheck()
{
    akashi::CommandRegistry l_registry;
    QVERIFY(!l_registry.contains("nonexistent"));
}

void tst_CommandRegistry::commandNamesList()
{
    akashi::CommandRegistry l_registry;
    l_registry.registerCommand({QStringLiteral("alpha"), {}, {}, 0, {}, {}}, [](akashi::CommandContext &) {});
    l_registry.registerCommand({QStringLiteral("beta"), {}, {}, 0, {}, {}}, [](akashi::CommandContext &) {});

    QStringList l_names = l_registry.commandNames();
    QCOMPARE(l_names.size(), 2);
    QVERIFY(l_names.contains("alpha"));
    QVERIFY(l_names.contains("beta"));
}

void tst_CommandRegistry::permissionRegistration()
{
    akashi::PermissionRegistry l_registry;
    akashi::PermissionInfo l_info{QStringLiteral("kick"), QStringLiteral("Kick"), {}, QStringLiteral("moderation")};
    QVERIFY(l_registry.registerPermission(l_info));
    QVERIFY(l_registry.isRegistered("kick"));

    if (auto l_found = l_registry.permissionById("kick")) {
        QCOMPARE(l_found->display_name, QStringLiteral("Kick"));
    }
    else {
        QFAIL("permission not found");
    }
}

void tst_CommandRegistry::permissionByCategory()
{
    akashi::PermissionRegistry l_registry;
    l_registry.registerPermission({QStringLiteral("kick"), {}, {}, QStringLiteral("moderation")});
    l_registry.registerPermission({QStringLiteral("ban"), {}, {}, QStringLiteral("moderation")});
    l_registry.registerPermission({QStringLiteral("motd"), {}, {}, QStringLiteral("admin")});

    auto l_mod = l_registry.permissionsByCategory("moderation");
    QCOMPARE(l_mod.size(), 2);

    auto l_admin = l_registry.permissionsByCategory("admin");
    QCOMPARE(l_admin.size(), 1);
}

void tst_CommandRegistry::permissionUnregisterByOwner()
{
    akashi::PermissionRegistry l_registry;
    l_registry.registerPermission({QStringLiteral("custom.perm"), {}, {}, {}}, QStringLiteral("plugin_x"));
    QVERIFY(l_registry.isRegistered("custom.perm"));
    l_registry.unregisterAllPermissions(QStringLiteral("plugin_x"));
    QVERIFY(!l_registry.isRegistered("custom.perm"));
}

void tst_CommandRegistry::duplicatePermissionFails()
{
    akashi::PermissionRegistry l_registry;
    l_registry.registerPermission({QStringLiteral("kick"), {}, {}, {}});
    QVERIFY(!l_registry.registerPermission({QStringLiteral("kick"), {}, {}, {}}));
}

void tst_CommandRegistry::resolverChainPriorityOrder()
{
    akashi::PermissionRegistry l_registry;
    QStringList l_call_order;

    l_registry.registerResolver(QStringLiteral("second"), 200, [&](const akashi::PermissionQuery &) {
        l_call_order.append("second");
        return akashi::PermissionVerdict::NoOpinion;
    });
    l_registry.registerResolver(QStringLiteral("first"), 100, [&](const akashi::PermissionQuery &) {
        l_call_order.append("first");
        return akashi::PermissionVerdict::NoOpinion;
    });
    l_registry.registerResolver(QStringLiteral("third"), 300, [&](const akashi::PermissionQuery &) {
        l_call_order.append("third");
        return akashi::PermissionVerdict::NoOpinion;
    });

    akashi::PermissionQuery l_query;
    l_query.permission = QStringLiteral("kick");
    l_registry.resolve(l_query);

    QCOMPARE(l_call_order, QStringList({"first", "second", "third"}));
}

void tst_CommandRegistry::resolverGrantedShortCircuits()
{
    akashi::PermissionRegistry l_registry;
    bool l_second_called = false;

    l_registry.registerResolver(QStringLiteral("granter"), 100, [](const akashi::PermissionQuery &) {
        return akashi::PermissionVerdict::Granted;
    });
    l_registry.registerResolver(QStringLiteral("never_reached"), 200, [&](const akashi::PermissionQuery &) {
        l_second_called = true;
        return akashi::PermissionVerdict::Denied;
    });

    akashi::PermissionQuery l_query;
    l_query.permission = QStringLiteral("kick");
    QVERIFY(l_registry.resolve(l_query));
    QVERIFY(!l_second_called);
}

void tst_CommandRegistry::resolverDeniedShortCircuits()
{
    akashi::PermissionRegistry l_registry;
    bool l_second_called = false;

    l_registry.registerResolver(QStringLiteral("denier"), 100, [](const akashi::PermissionQuery &) {
        return akashi::PermissionVerdict::Denied;
    });
    l_registry.registerResolver(QStringLiteral("never_reached"), 200, [&](const akashi::PermissionQuery &) {
        l_second_called = true;
        return akashi::PermissionVerdict::Granted;
    });

    akashi::PermissionQuery l_query;
    l_query.permission = QStringLiteral("kick");
    QVERIFY(!l_registry.resolve(l_query));
    QVERIFY(!l_second_called);
}

void tst_CommandRegistry::resolverAllNoOpinionDenies()
{
    akashi::PermissionRegistry l_registry;
    l_registry.registerResolver(QStringLiteral("shrug"), 100, [](const akashi::PermissionQuery &) {
        return akashi::PermissionVerdict::NoOpinion;
    });

    akashi::PermissionQuery l_query;
    l_query.permission = QStringLiteral("kick");
    QVERIFY(!l_registry.resolve(l_query));
}

void tst_CommandRegistry::resolverUnregisterByOwner()
{
    akashi::PermissionRegistry l_registry;
    l_registry.registerResolver(QStringLiteral("plugin_resolver"), 100, [](const akashi::PermissionQuery &) {
        return akashi::PermissionVerdict::Granted;
    }, QStringLiteral("plugin_x"));

    akashi::PermissionQuery l_query;
    l_query.permission = QStringLiteral("custom");
    QVERIFY(l_registry.resolve(l_query));

    l_registry.unregisterAllResolvers(QStringLiteral("plugin_x"));
    QVERIFY(!l_registry.resolve(l_query));
}

void tst_CommandRegistry::pluginResolverAtCustomPriority()
{
    akashi::PermissionRegistry l_registry;

    // Core denier at 300.
    l_registry.registerResolver(QStringLiteral("core_deny"), 300, [](const akashi::PermissionQuery &) {
        return akashi::PermissionVerdict::Denied;
    }, QStringLiteral("core"));

    // Plugin granter at 150 — runs before the core denier.
    l_registry.registerResolver(QStringLiteral("plugin_grant"), 150, [](const akashi::PermissionQuery &q) {
        if (q.permission == QStringLiteral("plugin.special")) {
            return akashi::PermissionVerdict::Granted;
        }
        return akashi::PermissionVerdict::NoOpinion;
    }, QStringLiteral("my_plugin"));

    akashi::PermissionQuery l_special;
    l_special.permission = QStringLiteral("plugin.special");
    QVERIFY(l_registry.resolve(l_special));

    akashi::PermissionQuery l_other;
    l_other.permission = QStringLiteral("kick");
    QVERIFY(!l_registry.resolve(l_other));
}

void tst_CommandRegistry::aclRoleStringCanPerform()
{
    ACLRole l_role;
    l_role.setPermission(QStringLiteral("kick"), true);
    l_role.setPermission(QStringLiteral("ban"), true);

    QVERIFY(l_role.canPerform(QStringLiteral("kick")));
    QVERIFY(l_role.canPerform(QStringLiteral("ban")));
    QVERIFY(!l_role.canPerform(QStringLiteral("mute")));
    QVERIFY(l_role.canPerform(QStringLiteral("none")));
    QVERIFY(l_role.canPerform(QStringLiteral("")));
}

void tst_CommandRegistry::aclRoleSuperWildcard()
{
    ACLRole l_role;
    l_role.setPermission(QStringLiteral("super"), true);

    QVERIFY(l_role.canPerform(QStringLiteral("kick")));
    QVERIFY(l_role.canPerform(QStringLiteral("ban")));
    QVERIFY(l_role.canPerform(QStringLiteral("anything")));
}

void tst_CommandRegistry::aclRoleEmptyIsNone()
{
    ACLRole l_role;
    QVERIFY(l_role.canPerform(QStringLiteral("none")));
    QVERIFY(l_role.canPerform(QStringLiteral("")));
    QVERIFY(!l_role.canPerform(QStringLiteral("kick")));
}

void tst_CommandRegistry::applyExtensionsAddsAliases()
{
    akashi::CommandRegistry l_registry;
    l_registry.registerCommand({QStringLiteral("roll"), {}, {}, 0, {}, {}}, [](akashi::CommandContext &) {});

    QTemporaryDir l_dir;
    QVERIFY(l_dir.isValid());
    const QString l_path = l_dir.filePath(QStringLiteral("ext.json"));
    QFile l_file(l_path);
    QVERIFY(l_file.open(QIODevice::WriteOnly));
    l_file.write(R"({"roll": {"aliases": "r dice"}})");
    l_file.close();

    l_registry.applyExtensions(l_path);

    QVERIFY(l_registry.contains("r"));
    QVERIFY(l_registry.contains("dice"));
    auto l_spec = l_registry.spec("r");
    QVERIFY(l_spec.has_value());
    QCOMPARE(l_spec->name, QStringLiteral("roll"));
}

void tst_CommandRegistry::applyExtensionsOverridesPermissions()
{
    akashi::CommandRegistry l_registry;
    l_registry.registerCommand({QStringLiteral("kick"), {}, {QStringLiteral("moderation.kick")}, 0, {}, {}}, [](akashi::CommandContext &) {});

    QTemporaryDir l_dir;
    QVERIFY(l_dir.isValid());
    const QString l_path = l_dir.filePath(QStringLiteral("ext.json"));
    QFile l_file(l_path);
    QVERIFY(l_file.open(QIODevice::WriteOnly));
    l_file.write(R"({"kick": {"permissions": "admin.kick super"}})");
    l_file.close();

    l_registry.applyExtensions(l_path);

    auto l_spec = l_registry.spec("kick");
    QVERIFY(l_spec.has_value());
    QCOMPARE(l_spec->permissions, QStringList({QStringLiteral("admin.kick"), QStringLiteral("super")}));
}

void tst_CommandRegistry::applyExtensionsSkipsUnknownCommand()
{
    akashi::CommandRegistry l_registry;
    l_registry.registerCommand({QStringLiteral("kick"), {}, {}, 0, {}, {}}, [](akashi::CommandContext &) {});

    QTemporaryDir l_dir;
    QVERIFY(l_dir.isValid());
    const QString l_path = l_dir.filePath(QStringLiteral("ext.json"));
    QFile l_file(l_path);
    QVERIFY(l_file.open(QIODevice::WriteOnly));
    l_file.write(R"({"nonexistent": {"aliases": "nx"}})");
    l_file.close();

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("unknown command"));
    l_registry.applyExtensions(l_path);

    QVERIFY(!l_registry.contains("nx"));
}

void tst_CommandRegistry::applyExtensionsSkipsCollidingAlias()
{
    akashi::CommandRegistry l_registry;
    l_registry.registerCommand({QStringLiteral("kick"), {QStringLiteral("k")}, {}, 0, {}, {}}, [](akashi::CommandContext &) {});
    l_registry.registerCommand({QStringLiteral("ban"), {}, {}, 0, {}, {}}, [](akashi::CommandContext &) {});

    QTemporaryDir l_dir;
    QVERIFY(l_dir.isValid());
    const QString l_path = l_dir.filePath(QStringLiteral("ext.json"));
    QFile l_file(l_path);
    QVERIFY(l_file.open(QIODevice::WriteOnly));
    l_file.write(R"({"ban": {"aliases": "k fresh"}})");
    l_file.close();

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("collides"));
    l_registry.applyExtensions(l_path);

    QVERIFY(l_registry.contains("fresh"));
    QCOMPARE(l_registry.spec("fresh")->name, QStringLiteral("ban"));
    auto l_k_spec = l_registry.spec("k");
    QVERIFY(l_k_spec.has_value());
    QCOMPARE(l_k_spec->name, QStringLiteral("kick"));
}

void tst_CommandRegistry::applyExtensionsMissingFileIsNoop()
{
    akashi::CommandRegistry l_registry;
    l_registry.registerCommand({QStringLiteral("test"), {}, {}, 0, {}, {}}, [](akashi::CommandContext &) {});

    l_registry.applyExtensions(QStringLiteral("/nonexistent/path/ext.json"));

    QVERIFY(l_registry.contains("test"));
}

void tst_CommandRegistry::applyExtensionsSilentlySkipsSameCommandAlias()
{
    akashi::CommandRegistry l_registry;
    l_registry.registerCommand({QStringLiteral("roll"), {QStringLiteral("r")}, {}, 0, {}, {}}, [](akashi::CommandContext &) {});

    QTemporaryDir l_dir;
    QVERIFY(l_dir.isValid());
    const QString l_path = l_dir.filePath(QStringLiteral("ext.json"));
    QFile l_file(l_path);
    QVERIFY(l_file.open(QIODevice::WriteOnly));
    l_file.write(R"({"roll": {"aliases": "r dice"}})");
    l_file.close();

    l_registry.applyExtensions(l_path);

    QVERIFY(l_registry.contains("r"));
    QVERIFY(l_registry.contains("dice"));
    QCOMPARE(l_registry.spec("r")->name, QStringLiteral("roll"));
    QCOMPARE(l_registry.spec("dice")->name, QStringLiteral("roll"));
}

} // namespace unittests
} // namespace tests

QTEST_MAIN(tests::unittests::tst_CommandRegistry)
#include "tst_command_registry.moc"
