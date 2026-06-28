// AI-generated: written by Claude.
#include "akashi/plugin.h"
#include "akashi/service_registry.h"
#include "core/command_registry.h"
#include "core/event_bus.h"
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
    explicit StubPlugin(const QString &f_id) : m_id(f_id) {}

    QString id() const override { return m_id; }
    akashi::ServiceVersion pluginVersion() const override { return {1, 0, 0}; }

    bool load(akashi::ServiceRegistry &) override { load_order.append(m_id); return m_load_result; }
    bool init(akashi::ServiceRegistry &) override { init_order.append(m_id); return m_init_result; }
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

class tst_PluginManager : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void init();

    void serviceIdentity();
    void emptyDirectorySucceeds();
    void nonexistentDirectorySucceeds();
    void pluginInfoForUnknownReturnsNullopt();
    void dependentsBlockUnload();
    void cleanupRemovesRegistrations();

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
    QFile l_file(l_path);
    if (!l_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
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

void tst_PluginManager::dependentsBlockUnload()
{
    QTemporaryDir l_tmp;
    akashi::PluginManager l_mgr(&m_services, l_tmp.path());
    l_mgr.startPlugins();
    QVERIFY(!l_mgr.unloadPlugin(QStringLiteral("nonexistent")));
}

void tst_PluginManager::cleanupRemovesRegistrations()
{
    akashi::ServiceRegistry l_services;
    auto l_commands = std::make_shared<akashi::CommandRegistry>();
    l_services.registerService(l_commands);

    akashi::CommandSpec l_spec;
    l_spec.name = QStringLiteral("test_cmd");
    l_spec.permissions = {QStringLiteral("none")};
    l_commands->registerCommand(l_spec, [](akashi::CommandContext &) {}, QStringLiteral("test.plugin"));

    QVERIFY(l_commands->contains(QStringLiteral("test_cmd")));
    l_commands->unregisterAll(QStringLiteral("test.plugin"));
    QVERIFY(!l_commands->contains(QStringLiteral("test_cmd")));
}

void tst_PluginManager::headerParsesFullManifest()
{
    QTemporaryDir l_tmp;
    const QString l_path = writeScript(l_tmp, "greeter.lua", R"(--[[ akashi-plugin
{
    "id": "akashi.greeter",
    "version": "2.3.4",
    "dependencies": ["akashi.other"],
    "services": ["akashi.commands"]
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

    akashi::PluginManager l_mgr(&m_services, l_tmp.path());
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("needs the host plugin"));
    QVERIFY(l_mgr.startPlugins());
    QVERIFY(!l_mgr.pluginInfo(QStringLiteral("akashi.orphan")).has_value());
}

} // namespace unittests
} // namespace tests

QTEST_GUILESS_MAIN(tests::unittests::tst_PluginManager)

#include "tst_plugin_manager.moc"
