// AI-generated: written by Claude.
#include "core/command_context.h"
#include "core/command_registry.h"
#include "core/command_spec.h"
#include "core/permission_registry.h"

#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

using akashi::ACLRole;

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
    void collidingAliasRegistrationFails();
    void unregisterByOwner();
    void unregisterUnknownOwnerIsANoOp();
    void containsCheck();
    void commandNamesList();

    void aclRoleStringCanPerform();
    void aclRoleSuperWildcard();
    void aclRoleEmptyIsNone();

    void applyExtensionsAddsAliases();
    void applyExtensionsOverridesPermissions();
    void applyExtensionsParsesAndGroupsAndValidates();
    void requirementGroupsExpressAnd();
    void shadowsStackOrderAndSweep();
    void applyExtensionsSkipsUnknownCommand();
    void applyExtensionsSkipsCollidingAlias();
    void applyExtensionsMissingFileIsNoop();
    void applyExtensionsSilentlySkipsSameCommandAlias();

    void canUseMirrorsTheDispatchGate();
    void canUseChecksEveryVariantForm();

    void variantMatchPicksByArgCount();
    void registerCommandValidatesVariants();
    void registerVariantAppendsGatedForm();
    void registerVariantRefusals();
    void registerVariantRejectsMalformedForms();
    void unregisterAllSweepsAddedVariants();
    void applyExtensionsOverridesVariantPermissions();
};

// A two-form command like /listperms: a free self form and a gated
// one-or-more-argument form.
static akashi::CommandSpec makeVariantSpec(const QString &f_name)
{
    return akashi::CommandSpec{
        .name = f_name,
        .usage = "/" + f_name + " [target]",
        .variants = {
            {QStringLiteral("own"), 0, 0, {}, {}, {}, [](akashi::CommandContext &) {}},
            {QStringLiteral("user"), 1, -1, {QStringLiteral("modify_users")}, {}, {}, [](akashi::CommandContext &) {}},
        },
    };
}

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

void tst_CommandRegistry::collidingAliasRegistrationFails()
{
    akashi::CommandRegistry l_registry;
    akashi::CommandSpec l_first{QStringLiteral("kick"), {QStringLiteral("k")}, {}, 0, {}, {}};
    QVERIFY(l_registry.registerCommand(l_first, [](akashi::CommandContext &) {}));

    // A new command whose name shadows an existing alias is refused.
    akashi::CommandSpec l_name_clash{QStringLiteral("k"), {}, {}, 0, {}, {}};
    QVERIFY(!l_registry.registerCommand(l_name_clash, [](akashi::CommandContext &) {}));

    // So is one whose alias shadows an existing command name.
    akashi::CommandSpec l_alias_clash{QStringLiteral("boot"), {QStringLiteral("kick")}, {}, 0, {}, {}};
    QVERIFY(!l_registry.registerCommand(l_alias_clash, [](akashi::CommandContext &) {}));

    // The refusal leaves nothing behind - not even the fresh name.
    QVERIFY(!l_registry.contains("boot"));
    QCOMPARE(l_registry.spec("k")->name, QStringLiteral("kick"));
}

void tst_CommandRegistry::unregisterByOwner()
{
    akashi::CommandRegistry l_registry;
    akashi::CommandSpec l_spec{QStringLiteral("test"), {QStringLiteral("t")}, {}, 0, {}, {}};
    QVERIFY(l_registry.registerCommand(
        l_spec, [](akashi::CommandContext &) {}, QStringLiteral("plugin_a")));
    QVERIFY(l_registry.contains("test"));
    QVERIFY(l_registry.contains("t"));

    l_registry.unregisterAll(QStringLiteral("plugin_a"));
    QVERIFY(!l_registry.contains("test"));
    QVERIFY(!l_registry.contains("t"));
}

void tst_CommandRegistry::unregisterUnknownOwnerIsANoOp()
{
    akashi::CommandRegistry l_registry;
    akashi::CommandSpec l_spec{QStringLiteral("test"), {QStringLiteral("t")}, {}, 0, {}, {}};
    QVERIFY(l_registry.registerCommand(
        l_spec, [](akashi::CommandContext &) {}, QStringLiteral("plugin_a")));
    QVERIFY(l_registry.registerCommand(makeVariantSpec(QStringLiteral("listperms")), QStringLiteral("plugin_a")));

    l_registry.unregisterAll(QStringLiteral("nobody"));

    QVERIFY(l_registry.contains("test"));
    QVERIFY(l_registry.contains("t"));
    QCOMPARE(l_registry.spec("listperms")->variants.size(), 2);
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
    QCOMPARE(l_spec->gate.permissions(), QStringList({QStringLiteral("admin.kick"), QStringLiteral("super")}));
}

void tst_CommandRegistry::applyExtensionsParsesAndGroupsAndValidates()
{
    akashi::CommandRegistry l_registry;
    l_registry.registerCommand({QStringLiteral("probe"), {}, {QStringLiteral("kick")}, 0, {}, {}}, [](akashi::CommandContext &) {});

    QTemporaryDir l_dir;
    QVERIFY(l_dir.isValid());
    const auto l_write = [&l_dir](const char *f_name, const QByteArray &f_json) -> QString {
        const QString l_path = l_dir.filePath(f_name);
        QFile l_file(l_path);
        if (!l_file.open(QIODevice::WriteOnly)) {
            return {};
        }
        l_file.write(f_json);
        return l_path;
    };
    const auto l_known = [](const QString &f_permission) {
        static const QSet<QString> s_known{QStringLiteral("gamemaster"), QStringLiteral("kick"), QStringLiteral("ban")};
        return s_known.contains(f_permission);
    };

    // "gamemaster+kick ban" reads as (gamemaster AND kick) OR ban.
    l_registry.applyExtensions(l_write("groups.json", R"({"probe": {"permissions": "gamemaster+kick ban"}})"), l_known);
    auto l_spec = l_registry.spec("probe");
    QVERIFY(l_spec.has_value());
    QCOMPARE(l_spec->gate.groups.size(), 2);
    QCOMPARE(l_spec->gate.groups.at(0), QStringList({QStringLiteral("gamemaster"), QStringLiteral("kick")}));
    QCOMPARE(l_spec->gate.groups.at(1), QStringList({QStringLiteral("ban")}));
    QCOMPARE(l_spec->gate.describe(), QStringLiteral("gamemaster+kick, ban"));

    // An override naming an unknown permission is skipped whole - a
    // half-applied gate could be softer than the author intended.
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("unknown permission")));
    l_registry.applyExtensions(l_write("typo.json", R"({"probe": {"permissions": "gamemaster+nosuchperm"}})"), l_known);
    l_spec = l_registry.spec("probe");
    QCOMPARE(l_spec->gate.groups.size(), 2);
}

void tst_CommandRegistry::requirementGroupsExpressAnd()
{
    // A requirement is any-of over all-of groups; the dispatcher and
    // canUse share the same answer.
    const auto l_holding = [](const QStringList &f_held) {
        return [f_held](const QString &f_permission) { return f_held.contains(f_permission); };
    };
    akashi::Gate l_gate = akashi::Gate::allOf({QStringLiteral("gamemaster"), QStringLiteral("kick")});
    l_gate.groups.append(QStringList{QStringLiteral("super")});
    QVERIFY(l_gate.passes(l_holding({QStringLiteral("gamemaster"), QStringLiteral("kick")})));
    QVERIFY(l_gate.passes(l_holding({QStringLiteral("super")})));
    QVERIFY(!l_gate.passes(l_holding({QStringLiteral("gamemaster")})));
    QVERIFY(!l_gate.passes(l_holding({QStringLiteral("kick")})));
    QCOMPARE(l_gate.permissions(), (QStringList{QStringLiteral("gamemaster"), QStringLiteral("kick"), QStringLiteral("super")}));

    // One name is a group of one, so the ordinary any-of case reads as a
    // plain list and decides the same way.
    const akashi::Gate l_any{QStringLiteral("kick"), QStringLiteral("ban")};
    QVERIFY(l_any.passes(l_holding({QStringLiteral("kick")})));
    QVERIFY(l_any.passes(l_holding({QStringLiteral("ban")})));
    QVERIFY(!l_any.passes(l_holding({QStringLiteral("gamemaster")})));

    // An empty gate is an open command.
    QVERIFY(akashi::Gate().passes(l_holding({})));
    QCOMPARE(akashi::Gate().describe(), QStringLiteral("nothing"));

    // A variant carrying a group gates canUse the same way.
    akashi::CommandRegistry l_registry;
    akashi::CommandSpec l_spec;
    l_spec.name = QStringLiteral("gated");
    l_spec.variants = {
        {.id = QStringLiteral("own"), .min_args = 1, .max_args = 1, .gate = {QStringLiteral("gamemaster")}, .handler = [](akashi::CommandContext &) {}},
        {.id = QStringLiteral("cross"), .min_args = 2, .max_args = -1, .gate = akashi::Gate::allOf({QStringLiteral("gamemaster"), QStringLiteral("kick")}), .handler = [](akashi::CommandContext &) {}},
    };
    QVERIFY(l_registry.registerCommand(l_spec, QStringLiteral("core")));
    QVERIFY(l_registry.canUse(QStringLiteral("gated"), l_holding({QStringLiteral("gamemaster")})));
    QVERIFY(!l_registry.canUse(QStringLiteral("gated"), l_holding({QStringLiteral("kick")})));
}

void tst_CommandRegistry::shadowsStackOrderAndSweep()
{
    akashi::CommandRegistry l_registry;
    l_registry.registerCommand({QStringLiteral("probe"), {QStringLiteral("prb")}, {}, 0, {}, {}}, [](akashi::CommandContext &) {}, QStringLiteral("core"));

    // An unknown command takes no shadow, and neither does a null one.
    QVERIFY(!l_registry.shadowCommand(QStringLiteral("nosuchverb"), 10, [](akashi::CommandContext &, const akashi::CommandNext &) {}, QStringLiteral("p1")));
    QVERIFY(!l_registry.shadowCommand(QStringLiteral("probe"), 10, {}, QStringLiteral("p1")));

    // Higher priority sits outermost; equal priorities keep registration
    // order; an alias resolves to the same stack.
    const auto l_noop = [](akashi::CommandContext &, const akashi::CommandNext &) {};
    QVERIFY(l_registry.shadowCommand(QStringLiteral("probe"), 10, l_noop, QStringLiteral("low")));
    QVERIFY(l_registry.shadowCommand(QStringLiteral("prb"), 20, l_noop, QStringLiteral("high_old")));
    QVERIFY(l_registry.shadowCommand(QStringLiteral("probe"), 20, l_noop, QStringLiteral("high_new")));
    QList<akashi::CommandShadow> l_stack = l_registry.shadowsOf(QStringLiteral("prb"));
    QCOMPARE(l_stack.size(), 3);
    QCOMPARE(l_stack.at(0).owner_id, QStringLiteral("high_old"));
    QCOMPARE(l_stack.at(1).owner_id, QStringLiteral("high_new"));
    QCOMPARE(l_stack.at(2).owner_id, QStringLiteral("low"));

    // A shadow leaves with its owner; the others keep their places.
    l_registry.unregisterAll(QStringLiteral("high_old"));
    l_stack = l_registry.shadowsOf(QStringLiteral("probe"));
    QCOMPARE(l_stack.size(), 2);
    QCOMPARE(l_stack.at(0).owner_id, QStringLiteral("high_new"));

    // The stack dies with its command, so a later command re-registered
    // under the same name starts unshadowed.
    l_registry.unregisterAll(QStringLiteral("core"));
    l_registry.registerCommand({QStringLiteral("probe"), {}, {}, 0, {}, {}}, [](akashi::CommandContext &) {}, QStringLiteral("core2"));
    QVERIFY(l_registry.shadowsOf(QStringLiteral("probe")).isEmpty());
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

void tst_CommandRegistry::canUseMirrorsTheDispatchGate()
{
    akashi::CommandRegistry l_registry;
    akashi::CommandSpec l_open{QStringLiteral("about"), {}, {}, 0, {}, {}};
    akashi::CommandSpec l_gated{QStringLiteral("ban"), {QStringLiteral("b")}, {QStringLiteral("ban")}, 3, {}, {}};
    QVERIFY(l_registry.registerCommand(l_open, [](akashi::CommandContext &) {}));
    QVERIFY(l_registry.registerCommand(l_gated, [](akashi::CommandContext &) {}));

    const auto l_none = [](const QString &) { return false; };
    const auto l_mod = [](const QString &f_permission) { return f_permission == QStringLiteral("ban"); };

    // An empty permission list is open, exactly like the dispatch gate.
    QVERIFY(l_registry.canUse("about", l_none));
    QVERIFY(!l_registry.canUse("ban", l_none));
    QVERIFY(l_registry.canUse("ban", l_mod));
    // Aliases resolve like every other lookup; unknown commands never pass.
    QVERIFY(l_registry.canUse("b", l_mod));
    QVERIFY(!l_registry.canUse("nosuch", l_mod));
}

void tst_CommandRegistry::canUseChecksEveryVariantForm()
{
    akashi::CommandRegistry l_registry;
    QVERIFY(l_registry.registerCommand(makeVariantSpec(QStringLiteral("listperms")), QStringLiteral("core")));

    // The free self form opens the command even without modify_users.
    QVERIFY(l_registry.canUse("listperms", [](const QString &) { return false; }));

    const akashi::CommandSpec l_all_gated{
        .name = QStringLiteral("uncm"),
        .variants = {
            {QStringLiteral("own"), 0, 0, {QStringLiteral("gamemaster")}, {}, {}, [](akashi::CommandContext &) {}},
            {QStringLiteral("other"), 1, -1, {QStringLiteral("remove_gamemaster")}, {}, {}, [](akashi::CommandContext &) {}},
        },
    };
    QVERIFY(l_registry.registerCommand(l_all_gated, QStringLiteral("core")));

    QVERIFY(!l_registry.canUse("uncm", [](const QString &) { return false; }));
    // Any one passing form is enough.
    QVERIFY(l_registry.canUse("uncm", [](const QString &f_permission) { return f_permission == QStringLiteral("remove_gamemaster"); }));
}

void tst_CommandRegistry::variantMatchPicksByArgCount()
{
    const akashi::CommandSpec l_spec = makeVariantSpec(QStringLiteral("listperms"));

    // First matching form in declaration order wins.
    QCOMPARE(l_spec.match(0)->id, QStringLiteral("own"));
    QCOMPARE(l_spec.match(1)->id, QStringLiteral("user"));
    QCOMPARE(l_spec.match(5)->id, QStringLiteral("user"));

    // Bounded windows reject counts outside them.
    const akashi::CommandSpec l_bounded{
        .variants = {
            {QStringLiteral("own"), 1, 1, {}, {}, {}, [](akashi::CommandContext &) {}},
            {QStringLiteral("user"), 2, 2, {}, {}, {}, [](akashi::CommandContext &) {}},
        },
    };
    QCOMPARE(l_bounded.match(0), nullptr);
    QCOMPARE(l_bounded.match(1)->id, QStringLiteral("own"));
    QCOMPARE(l_bounded.match(2)->id, QStringLiteral("user"));
    QCOMPARE(l_bounded.match(3), nullptr);
}

void tst_CommandRegistry::registerCommandValidatesVariants()
{
    akashi::CommandRegistry l_registry;

    // The handlerless overload needs at least one variant.
    const akashi::CommandSpec l_empty{.name = QStringLiteral("empty")};
    QVERIFY(!l_registry.registerCommand(l_empty, QStringLiteral("core")));

    // Every variant needs an id and a handler.
    akashi::CommandSpec l_no_id = makeVariantSpec(QStringLiteral("noid"));
    l_no_id.variants[0].id.clear();
    QVERIFY(!l_registry.registerCommand(l_no_id, QStringLiteral("core")));

    akashi::CommandSpec l_no_handler = makeVariantSpec(QStringLiteral("nohandler"));
    l_no_handler.variants[1].handler = {};
    QVERIFY(!l_registry.registerCommand(l_no_handler, QStringLiteral("core")));

    QVERIFY(l_registry.registerCommand(makeVariantSpec(QStringLiteral("listperms")), QStringLiteral("core")));
    QCOMPARE(l_registry.spec("listperms")->variants.size(), 2);
}

void tst_CommandRegistry::registerVariantAppendsGatedForm()
{
    akashi::CommandRegistry l_registry;
    const akashi::CommandSpec l_spec{
        .name = QStringLiteral("inspect"),
        .variants = {
            {QStringLiteral("own"), 0, 0, {}, {}, {}, [](akashi::CommandContext &) {}},
        },
    };
    QVERIFY(l_registry.registerCommand(l_spec, QStringLiteral("core")));

    // A plugin bolts a gated one-argument form onto the command.
    akashi::CommandVariant l_extra{QStringLiteral("other"), 1, 1, {QStringLiteral("my.permission")}, {}, {}, [](akashi::CommandContext &) {}};
    QVERIFY(l_registry.registerVariant(QStringLiteral("inspect"), l_extra, QStringLiteral("my-plugin")));

    const auto l_stored = l_registry.spec("inspect");
    QCOMPARE(l_stored->variants.size(), 2);
    QCOMPARE(l_stored->match(1)->id, QStringLiteral("other"));
    QCOMPARE(l_stored->match(1)->gate.permissions(), QStringList{QStringLiteral("my.permission")});
}

void tst_CommandRegistry::registerVariantRefusals()
{
    akashi::CommandRegistry l_registry;
    QVERIFY(l_registry.registerCommand(makeVariantSpec(QStringLiteral("listperms")), QStringLiteral("core")));
    l_registry.registerCommand({QStringLiteral("plain"), {}, {}, 0, {}, {}}, [](akashi::CommandContext &) {});

    const auto l_handler = [](akashi::CommandContext &) {};

    // Unknown command.
    QVERIFY(!l_registry.registerVariant(QStringLiteral("missing"), {QStringLiteral("x"), 0, 0, {}, {}, {}, l_handler}));

    // A command without variants keeps its single handler reachable.
    QVERIFY(!l_registry.registerVariant(QStringLiteral("plain"), {QStringLiteral("x"), 0, 0, {}, {}, {}, l_handler}));

    // Duplicate id.
    QVERIFY(!l_registry.registerVariant(QStringLiteral("listperms"), {QStringLiteral("own"), 2, 2, {}, {}, {}, l_handler}));

    // The unbounded "user" form already covers two arguments.
    QVERIFY(!l_registry.registerVariant(QStringLiteral("listperms"), {QStringLiteral("two"), 2, 2, {}, {}, {}, l_handler}));

    QCOMPARE(l_registry.spec("listperms")->variants.size(), 2);
}

void tst_CommandRegistry::registerVariantRejectsMalformedForms()
{
    akashi::CommandRegistry l_registry;
    QVERIFY(l_registry.registerCommand(makeVariantSpec(QStringLiteral("listperms")), QStringLiteral("core")));

    const auto l_handler = [](akashi::CommandContext &) {};

    // A form without an id or without a handler is refused.
    QVERIFY(!l_registry.registerVariant(QStringLiteral("listperms"), {QString(), 2, 2, {}, {}, {}, l_handler}));
    QVERIFY(!l_registry.registerVariant(QStringLiteral("listperms"), {QStringLiteral("mute"), 2, 2, {}, {}, {}, {}}));

    // An unbounded window swallows every count above it, so nothing can
    // sit beside the open-ended "user" form - not even far above it.
    QVERIFY(!l_registry.registerVariant(QStringLiteral("listperms"), {QStringLiteral("many"), 5, -1, {}, {}, {}, l_handler}));

    QCOMPARE(l_registry.spec("listperms")->variants.size(), 2);
}

void tst_CommandRegistry::unregisterAllSweepsAddedVariants()
{
    akashi::CommandRegistry l_registry;
    const akashi::CommandSpec l_spec{
        .name = QStringLiteral("inspect"),
        .variants = {
            {QStringLiteral("own"), 0, 0, {}, {}, {}, [](akashi::CommandContext &) {}},
        },
    };
    QVERIFY(l_registry.registerCommand(l_spec, QStringLiteral("core")));
    QVERIFY(l_registry.registerVariant(QStringLiteral("inspect"),
                                       {QStringLiteral("other"), 1, 1, {}, {}, {}, [](akashi::CommandContext &) {}},
                                       QStringLiteral("my-plugin")));

    l_registry.unregisterAll(QStringLiteral("my-plugin"));

    // The command survives; only the plugin's form is gone.
    const auto l_stored = l_registry.spec("inspect");
    QVERIFY(l_stored.has_value());
    QCOMPARE(l_stored->variants.size(), 1);
    QCOMPARE(l_stored->variants.first().id, QStringLiteral("own"));
}

void tst_CommandRegistry::applyExtensionsOverridesVariantPermissions()
{
    akashi::CommandRegistry l_registry;
    QVERIFY(l_registry.registerCommand(makeVariantSpec(QStringLiteral("listperms")), QStringLiteral("core")));

    QTemporaryDir l_dir;
    QVERIFY(l_dir.isValid());
    const QString l_path = l_dir.filePath(QStringLiteral("ext.json"));
    QFile l_file(l_path);
    QVERIFY(l_file.open(QIODevice::WriteOnly));
    l_file.write(R"({"listperms.user": {"permissions": "super"}, "listperms.missing": {"permissions": "super"}, "listperms": {"permissions": "super"}})");
    l_file.close();

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("gated forms"));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("no variant"));
    l_registry.applyExtensions(l_path);

    const auto l_stored = l_registry.spec("listperms");
    // The addressed form changed; the base override on a variant command
    // only warns, and the other form keeps its gate.
    QCOMPARE(l_stored->match(1)->gate.permissions(), QStringList{QStringLiteral("super")});
    QVERIFY(l_stored->match(0)->gate.isEmpty());
    QVERIFY(l_stored->gate.isEmpty());
}

} // namespace unittests
} // namespace tests

QTEST_MAIN(tests::unittests::tst_CommandRegistry)
#include "tst_command_registry.moc"
