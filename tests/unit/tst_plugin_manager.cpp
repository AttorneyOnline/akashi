// AI-generated: written by Claude.
#include "akashi/plugin.h"
#include "akashi/script_plugin_host.h"
#include "akashi/service_registry.h"
#include "core/command_registry.h"
#include "core/plugin_manager.h"

#include <QTemporaryDir>
#include <QTest>

namespace tests {
namespace unittests {

class StubPlugin : public QObject, public akashi::IPlugin
{
    Q_OBJECT
    Q_INTERFACES(akashi::IPlugin)

  public:
    explicit StubPlugin(const QString &f_id) :
        m_id(f_id) {}

    QString id() const override { return m_id; }
    akashi::ServiceVersion pluginVersion() const override { return {1, 0, 0}; }

    bool load(akashi::ServiceRegistry &) override
    {
        load_order.append(m_id);
        return m_load_result;
    }
    bool init(akashi::ServiceRegistry &) override
    {
        init_order.append(m_id);
        return m_init_result;
    }
    void started(akashi::ServiceRegistry &) override { started_order.append(m_id); }
    void shutdown(akashi::ServiceRegistry &) override { shutdown_order.append(m_id); }

    bool m_load_result = true;
    bool m_init_result = true;

    static QStringList load_order;
    static QStringList init_order;
    static QStringList started_order;
    static QStringList shutdown_order;

    static void resetOrder()
    {
        load_order.clear();
        init_order.clear();
        started_order.clear();
        shutdown_order.clear();
    }

  private:
    QString m_id;
};

QStringList StubPlugin::load_order;
QStringList StubPlugin::init_order;
QStringList StubPlugin::started_order;
QStringList StubPlugin::shutdown_order;

// A script host that reports whatever manifests a test hands it, so the
// manager's dependency handling runs without any real interpreter.
class StubScriptHost : public akashi::IScriptPluginHost
{
  public:
    QString serviceId() const override { return QStringLiteral("akashi.script-host.stub"); }
    akashi::ServiceVersion serviceVersion() const override { return {1, 0, 0}; }
    QString runtime() const override { return QStringLiteral("stub"); }

    QList<akashi::PluginInfo> discoverScriptPlugins(const QString &) override { return manifests; }
    bool loadScriptPlugin(const QString &f_plugin_id, const QString &) override
    {
        loaded.append(f_plugin_id);
        return true;
    }
    void unloadScriptPlugin(const QString &f_plugin_id) override { unloaded.append(f_plugin_id); }

    QList<akashi::PluginInfo> manifests;
    QStringList loaded;
    QStringList unloaded;
};

static akashi::PluginInfo makeManifest(const QString &f_id, const QStringList &f_dependencies)
{
    akashi::PluginInfo l_info;
    l_info.id = f_id;
    l_info.runtime = QStringLiteral("stub");
    l_info.entry_path = f_id + QStringLiteral(".stub");
    l_info.dependencies = f_dependencies;
    return l_info;
}

class tst_PluginManager : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void init();

    void serviceIdentity();
    void emptyDirectorySucceeds();
    void nonexistentDirectorySucceeds();
    void pluginInfoForUnknownReturnsNullopt();
    void unloadUnknownPluginRefuses();
    void dependentsBlockUnloadWithoutCascade();
    void scriptDependencyCyclesNeverLoad();
    void cleanupRemovesRegistrations();
    void aboutLeavesWithThePlugin();

    void headerParsesFullManifest();
    void headerDefaultsEverythingButTheMarker();
    void headerInfersRuntimeFromExtension();
    void headerKeepsExplicitDependencies();
    void headerSurvivesNestedBraces();
    void headerRejectsMissingMarker();
    void headerRejectsBadJson();
    void headerRejectsUnknownExtension();
    void discoverySkipsScriptWithoutHost();

  private:
    QString writeScript(QTemporaryDir &f_dir, const QString &f_name, const QByteArray &f_content);

    akashi::ServiceRegistry m_services;
};

QString tst_PluginManager::writeScript(QTemporaryDir &f_dir, const QString &f_name, const QByteArray &f_content)
{
    const QString l_path = f_dir.filePath(f_name);
    // Fails the test here instead of returning a silent empty path.
    QFile l_file(l_path);
    if (!QTest::qVerify(l_file.open(QIODevice::WriteOnly | QIODevice::Text), "l_file.open(QIODevice::WriteOnly | QIODevice::Text)", qPrintable(l_path), __FILE__, __LINE__)) {
        return {};
    }
    l_file.write(f_content);
    l_file.close();
    return l_path;
}

void tst_PluginManager::init()
{
    StubPlugin::resetOrder();
}

void tst_PluginManager::serviceIdentity()
{
    QTemporaryDir l_tmp;
    akashi::PluginManager l_mgr(&m_services, l_tmp.path());
    QCOMPARE(l_mgr.serviceId(), QStringLiteral("akashi.plugins"));
    QCOMPARE(l_mgr.serviceVersion().major, 1);
}

void tst_PluginManager::emptyDirectorySucceeds()
{
    QTemporaryDir l_tmp;
    akashi::PluginManager l_mgr(&m_services, l_tmp.path());
    QVERIFY(l_mgr.startPlugins());
    QVERIFY(l_mgr.plugins().isEmpty());
}

void tst_PluginManager::nonexistentDirectorySucceeds()
{
    akashi::PluginManager l_mgr(&m_services, QStringLiteral("/nonexistent/path"));
    QVERIFY(l_mgr.startPlugins());
    QVERIFY(l_mgr.plugins().isEmpty());
}

void tst_PluginManager::pluginInfoForUnknownReturnsNullopt()
{
    QTemporaryDir l_tmp;
    akashi::PluginManager l_mgr(&m_services, l_tmp.path());
    QVERIFY(!l_mgr.pluginInfo(QStringLiteral("nonexistent")).has_value());
}

void tst_PluginManager::unloadUnknownPluginRefuses()
{
    QTemporaryDir l_tmp;
    akashi::PluginManager l_mgr(&m_services, l_tmp.path());
    l_mgr.startPlugins();
    QVERIFY(!l_mgr.unloadPlugin(QStringLiteral("nonexistent")));
}

void tst_PluginManager::dependentsBlockUnloadWithoutCascade()
{
    QTemporaryDir l_tmp;
    akashi::ServiceRegistry l_services;
    auto l_host = std::make_shared<StubScriptHost>();
    l_host->manifests = {makeManifest(QStringLiteral("akashi.base"), {}),
                         makeManifest(QStringLiteral("akashi.child"), {QStringLiteral("akashi.base")})};
    QVERIFY(l_services.registerService(l_host));

    akashi::PluginManager l_mgr(&l_services, l_tmp.path());
    QVERIFY(l_mgr.startPlugins());
    QCOMPARE(l_mgr.pluginInfo(QStringLiteral("akashi.base"))->state, akashi::PluginInfo::State::Started);
    QCOMPARE(l_mgr.pluginInfo(QStringLiteral("akashi.child"))->state, akashi::PluginInfo::State::Started);

    // The child still depends on the base, so the base refuses to go alone
    // and nothing is torn down.
    QVERIFY(!l_mgr.unloadPlugin(QStringLiteral("akashi.base")));
    QCOMPARE(l_mgr.pluginInfo(QStringLiteral("akashi.base"))->state, akashi::PluginInfo::State::Started);
    QVERIFY(l_host->unloaded.isEmpty());

    // Cascading takes the child down first, then the base.
    QVERIFY(l_mgr.unloadPlugin(QStringLiteral("akashi.base"), true));
    QCOMPARE(l_host->unloaded, QStringList({QStringLiteral("akashi.child"), QStringLiteral("akashi.base")}));
    QCOMPARE(l_mgr.pluginInfo(QStringLiteral("akashi.base"))->state, akashi::PluginInfo::State::Discovered);
    QCOMPARE(l_mgr.pluginInfo(QStringLiteral("akashi.child"))->state, akashi::PluginInfo::State::Discovered);
}

void tst_PluginManager::scriptDependencyCyclesNeverLoad()
{
    QTemporaryDir l_tmp;
    akashi::ServiceRegistry l_services;
    auto l_host = std::make_shared<StubScriptHost>();
    l_host->manifests = {makeManifest(QStringLiteral("akashi.self"), {QStringLiteral("akashi.self")}),
                         makeManifest(QStringLiteral("akashi.egg"), {QStringLiteral("akashi.chicken")}),
                         makeManifest(QStringLiteral("akashi.chicken"), {QStringLiteral("akashi.egg")})};
    QVERIFY(l_services.registerService(l_host));

    // A self-dependency or a cycle can never become ready: the load pass
    // stops when nothing moves, warns about each one, and startPlugins
    // still finishes instead of spinning.
    akashi::PluginManager l_mgr(&l_services, l_tmp.path());
    for (int i = 0; i < 3; i++) {
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("not started"));
    }
    QVERIFY(l_mgr.startPlugins());

    QVERIFY(l_host->loaded.isEmpty());
    QCOMPARE(l_mgr.pluginInfo(QStringLiteral("akashi.self"))->state, akashi::PluginInfo::State::Discovered);
    QCOMPARE(l_mgr.pluginInfo(QStringLiteral("akashi.egg"))->state, akashi::PluginInfo::State::Discovered);
    QCOMPARE(l_mgr.pluginInfo(QStringLiteral("akashi.chicken"))->state, akashi::PluginInfo::State::Discovered);

    // A cycle member also refuses a direct load while its partner is stuck.
    QVERIFY(!l_mgr.loadPlugin(QStringLiteral("akashi.egg")));
}

void tst_PluginManager::cleanupRemovesRegistrations()
{
    akashi::ServiceRegistry l_services;
    auto l_commands = std::make_shared<akashi::CommandRegistry>();
    l_services.registerService(l_commands);

    akashi::CommandSpec l_spec;
    l_spec.name = QStringLiteral("test_cmd");
    l_spec.permissions = {QStringLiteral("none")};
    l_commands->registerCommand(
        l_spec, [](akashi::CommandContext &) {}, QStringLiteral("test.plugin"));

    QVERIFY(l_commands->contains(QStringLiteral("test_cmd")));
    l_commands->unregisterAll(QStringLiteral("test.plugin"));
    QVERIFY(!l_commands->contains(QStringLiteral("test_cmd")));
}

void tst_PluginManager::aboutLeavesWithThePlugin()
{
    QTemporaryDir l_tmp;
    akashi::ServiceRegistry l_services;
    auto l_host = std::make_shared<StubScriptHost>();
    akashi::PluginInfo l_manifest = makeManifest(QStringLiteral("akashi.credited"), {});
    l_manifest.about = QStringLiteral("Made by the test suite.");
    l_host->manifests = {l_manifest};
    QVERIFY(l_services.registerService(l_host));

    akashi::PluginManager l_mgr(&l_services, l_tmp.path());
    QVERIFY(l_mgr.startPlugins());

    // The manifest's line shows while the plugin runs.
    QCOMPARE(l_mgr.abouts().value(QStringLiteral("akashi.credited")), QStringLiteral("Made by the test suite."));

    // A runtime override replaces it; unknown ids and empty text are refused.
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Refused about text"));
    l_mgr.registerAbout(QStringLiteral("akashi.ghost"), QStringLiteral("nobody"));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Refused about text"));
    l_mgr.registerAbout(QStringLiteral("akashi.credited"), QString());
    l_mgr.registerAbout(QStringLiteral("akashi.credited"), QStringLiteral("Runs on stub 1.0."));
    QCOMPARE(l_mgr.abouts().value(QStringLiteral("akashi.credited")), QStringLiteral("Runs on stub 1.0."));
    QCOMPARE(l_mgr.abouts().size(), 1);

    // An unloaded plugin's line leaves /about with it.
    QVERIFY(l_mgr.unloadPlugin(QStringLiteral("akashi.credited")));
    QVERIFY(l_mgr.abouts().isEmpty());
}

void tst_PluginManager::headerParsesFullManifest()
{
    QTemporaryDir l_tmp;
    const QString l_path = writeScript(l_tmp, "greeter.lua", R"(--[[ akashi-plugin
{
    "id": "akashi.greeter",
    "version": "2.3.4",
    "dependencies": ["akashi.other"],
    "services": ["akashi.commands"],
    "about": "The greeter, by example."
}
--]]
print("hi")
)");

    const auto l_info = akashi::PluginManager::parseScriptHeader(l_path);
    QVERIFY(l_info.has_value());
    QCOMPARE(l_info->id, QStringLiteral("akashi.greeter"));
    QCOMPARE(l_info->version.toString(), QStringLiteral("2.3.4"));
    QCOMPARE(l_info->runtime, QStringLiteral("lua"));
    QCOMPARE(l_info->entry_path, l_path);
    // The host dependency is added in front of the declared ones.
    QCOMPARE(l_info->dependencies, QStringList({"akashi.lua-host", "akashi.other"}));
    QCOMPARE(l_info->services, QStringList({"akashi.commands"}));
    QCOMPARE(l_info->about, QStringLiteral("The greeter, by example."));
}

void tst_PluginManager::headerDefaultsEverythingButTheMarker()
{
    QTemporaryDir l_tmp;
    const QString l_path = writeScript(l_tmp, "tiny.lua", "--[[ akashi-plugin {} ]]\n");

    const auto l_info = akashi::PluginManager::parseScriptHeader(l_path);
    QVERIFY(l_info.has_value());
    QCOMPARE(l_info->id, QStringLiteral("tiny"));
    QCOMPARE(l_info->version.toString(), QStringLiteral("1.0.0"));
    QCOMPARE(l_info->dependencies, QStringList({"akashi.lua-host"}));
    QVERIFY(l_info->services.isEmpty());
}

void tst_PluginManager::headerInfersRuntimeFromExtension()
{
    QTemporaryDir l_tmp;
    const QString l_path = writeScript(l_tmp, "tiny.py", "\"\"\"akashi-plugin {}\"\"\"\n");

    const auto l_info = akashi::PluginManager::parseScriptHeader(l_path);
    QVERIFY(l_info.has_value());
    QCOMPARE(l_info->runtime, QStringLiteral("python"));
    QCOMPARE(l_info->dependencies, QStringList({"akashi.python-host"}));
}

void tst_PluginManager::headerKeepsExplicitDependencies()
{
    QTemporaryDir l_tmp;
    const QString l_path = writeScript(l_tmp, "explicit.lua",
                                       R"(--[[ akashi-plugin {"dependencies": ["akashi.lua-host"]} ]])");

    const auto l_info = akashi::PluginManager::parseScriptHeader(l_path);
    QVERIFY(l_info.has_value());
    // A declared host dependency is not doubled.
    QCOMPARE(l_info->dependencies, QStringList({"akashi.lua-host"}));
}

void tst_PluginManager::headerSurvivesNestedBraces()
{
    QTemporaryDir l_tmp;
    const QString l_path = writeScript(l_tmp, "nested.lua",
                                       R"(--[[ akashi-plugin {"id": "akashi.nested", "extras": {"depth": {"more": 1}}} ]])");

    const auto l_info = akashi::PluginManager::parseScriptHeader(l_path);
    QVERIFY(l_info.has_value());
    QCOMPARE(l_info->id, QStringLiteral("akashi.nested"));
}

void tst_PluginManager::headerRejectsMissingMarker()
{
    QTemporaryDir l_tmp;
    const QString l_path = writeScript(l_tmp, "plain.lua", "print(\"just a script\")\n");

    QVERIFY(!akashi::PluginManager::parseScriptHeader(l_path).has_value());
}

void tst_PluginManager::headerRejectsBadJson()
{
    QTemporaryDir l_tmp;
    const QString l_unclosed = writeScript(l_tmp, "unclosed.lua", "--[[ akashi-plugin { \"id\": \"x\" ]]\n");
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("No declaration object"));
    QVERIFY(!akashi::PluginManager::parseScriptHeader(l_unclosed).has_value());

    const QString l_garbage = writeScript(l_tmp, "garbage.lua", "--[[ akashi-plugin {id: nope,} ]]\n");
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Unreadable declaration header"));
    QVERIFY(!akashi::PluginManager::parseScriptHeader(l_garbage).has_value());
}

void tst_PluginManager::headerRejectsUnknownExtension()
{
    QTemporaryDir l_tmp;
    const QString l_path = writeScript(l_tmp, "mystery.rb", "# akashi-plugin {}\n");

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("No runtime"));
    QVERIFY(!akashi::PluginManager::parseScriptHeader(l_path).has_value());
}

void tst_PluginManager::discoverySkipsScriptWithoutHost()
{
    QTemporaryDir l_tmp;
    writeScript(l_tmp, "orphan.lua", "--[[ akashi-plugin {\"id\": \"akashi.orphan\"} ]]\n");

    // Hosts do their own scanning; with no host running, nobody reports
    // the file and it never becomes a plugin.
    akashi::PluginManager l_mgr(&m_services, l_tmp.path());
    QVERIFY(l_mgr.startPlugins());
    QVERIFY(!l_mgr.pluginInfo(QStringLiteral("akashi.orphan")).has_value());
}

} // namespace unittests
} // namespace tests

QTEST_GUILESS_MAIN(tests::unittests::tst_PluginManager)

#include "tst_plugin_manager.moc"
