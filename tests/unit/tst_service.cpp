// AI-generated: written by Claude.
#include <QSignalSpy>
#include <QTest>

#include "akashi/service_registry.h"

namespace tests {
namespace unittests {

using namespace akashi;

// A stand-in service used to check registration and typed resolution.
class FakeMusicService : public IService
{
  public:
    QString serviceId() const override { return "akashi.music"; }
    ServiceVersion serviceVersion() const override { return {2, 1, 0}; }
    int trackCount() const { return 42; }
};

class OtherService : public IService
{
  public:
    QString serviceId() const override { return "akashi.other"; }
    ServiceVersion serviceVersion() const override { return {1, 0, 0}; }
};

class tst_Service : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void versionSatisfies_data();
    void versionSatisfies();
    void registerAndResolve();
    void duplicateIdIsRejected();
    void versionRangeFiltersFind();
    void unregisterByOwner();
    void signalsFireOnChange();
};

void tst_Service::versionSatisfies_data()
{
    QTest::addColumn<QString>("range");
    QTest::addColumn<bool>("expected");

    // The service under test is version 1.2.3.
    QTest::addRow("Empty always matches") << "" << true;
    QTest::addRow("At least, met") << ">=1.0.0" << true;
    QTest::addRow("At least, not met") << ">=2.0.0" << false;
    QTest::addRow("Below, met") << "<2.0.0" << true;
    QTest::addRow("Range met") << ">=1.0.0,<2.0.0" << true;
    QTest::addRow("Range not met") << ">=1.0.0,<1.2.0" << false;
    QTest::addRow("Caret same major") << "^1.0.0" << true;
    QTest::addRow("Caret next major") << "^2.0.0" << false;
    QTest::addRow("Exact met") << "1.2.3" << true;
    QTest::addRow("Exact not met") << "1.2.4" << false;
}

void tst_Service::versionSatisfies()
{
    QFETCH(QString, range);
    QFETCH(bool, expected);

    const ServiceVersion l_version{1, 2, 3};
    QCOMPARE(l_version.satisfies(range), expected);
}

void tst_Service::registerAndResolve()
{
    ServiceRegistry l_registry;
    QVERIFY(l_registry.registerService(std::make_shared<FakeMusicService>()));

    const std::shared_ptr<FakeMusicService> l_music = l_registry.resolve<FakeMusicService>("akashi.music");
    QVERIFY(l_music != nullptr);
    QCOMPARE(l_music->trackCount(), 42);

    // Resolving to the wrong type gives null.
    QVERIFY(l_registry.resolve<OtherService>("akashi.music") == nullptr);
}

void tst_Service::duplicateIdIsRejected()
{
    ServiceRegistry l_registry;
    QVERIFY(l_registry.registerService(std::make_shared<FakeMusicService>()));
    QCOMPARE(l_registry.registerService(std::make_shared<FakeMusicService>()), false);
}

void tst_Service::versionRangeFiltersFind()
{
    ServiceRegistry l_registry;
    l_registry.registerService(std::make_shared<FakeMusicService>()); // version 2.1.0

    QVERIFY(l_registry.isAvailable("akashi.music", ">=2.0.0"));
    QVERIFY(!l_registry.isAvailable("akashi.music", ">=3.0.0"));
    QVERIFY(l_registry.find("akashi.music", "^2.0.0") != nullptr);
    QVERIFY(l_registry.find("akashi.music", "^1.0.0") == nullptr);
}

void tst_Service::unregisterByOwner()
{
    ServiceRegistry l_registry;
    l_registry.registerService(std::make_shared<FakeMusicService>(), "core");
    l_registry.registerService(std::make_shared<OtherService>(), "myplugin");

    l_registry.unregisterServicesOwnedBy("myplugin");
    QVERIFY(l_registry.isAvailable("akashi.music"));
    QVERIFY(!l_registry.isAvailable("akashi.other"));
}

void tst_Service::signalsFireOnChange()
{
    ServiceRegistry l_registry;
    QSignalSpy l_registered(&l_registry, &ServiceRegistry::serviceRegistered);
    QSignalSpy l_unregistered(&l_registry, &ServiceRegistry::serviceUnregistered);

    l_registry.registerService(std::make_shared<FakeMusicService>());
    l_registry.unregisterService("akashi.music");

    QCOMPARE(l_registered.count(), 1);
    QCOMPARE(l_unregistered.count(), 1);
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_Service)

#include "tst_service.moc"
