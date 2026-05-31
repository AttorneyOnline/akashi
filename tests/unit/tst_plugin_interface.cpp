// AI-generated: written by Claude.
#include "akashi/plugin.h"
#include "akashi/service_registry.h"

#include <QTest>

namespace tests {
namespace unittests {

class MockPlugin : public QObject, public akashi::IPlugin
{
    Q_OBJECT
    Q_INTERFACES(akashi::IPlugin)

  public:
    QString id() const override { return QStringLiteral("test.mock"); }
    akashi::ServiceVersion pluginVersion() const override { return {0, 1, 0}; }
    bool load(akashi::ServiceRegistry &) override { m_loaded = true; return true; }
    void shutdown(akashi::ServiceRegistry &) override { m_shutdown = true; }

    bool m_loaded = false;
    bool m_shutdown = false;
};

class tst_PluginInterface : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void iidIsCorrect();
    void qobjectCastWorks();
    void defaultInitAndStartedAreNoOps();
};

void tst_PluginInterface::iidIsCorrect()
{
    QCOMPARE(QString::fromLatin1(qobject_interface_iid<akashi::IPlugin *>()), QStringLiteral("org.akashi.IPlugin/1"));
}

void tst_PluginInterface::qobjectCastWorks()
{
    MockPlugin l_plugin;
    auto *l_iface = qobject_cast<akashi::IPlugin *>(&l_plugin);
    QVERIFY(l_iface);
    QCOMPARE(l_iface->id(), QStringLiteral("test.mock"));
}

void tst_PluginInterface::defaultInitAndStartedAreNoOps()
{
    MockPlugin l_plugin;
    akashi::ServiceRegistry l_services;
    QVERIFY(l_plugin.init(l_services));
    l_plugin.started(l_services);
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_PluginInterface)

#include "tst_plugin_interface.moc"
