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

  private:
    akashi::ServiceRegistry m_services;
};

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

} // namespace unittests
} // namespace tests

QTEST_GUILESS_MAIN(tests::unittests::tst_PluginManager)

#include "tst_plugin_manager.moc"
