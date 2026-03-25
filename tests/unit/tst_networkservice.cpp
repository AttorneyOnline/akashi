// AI-generated: written by Claude.
#include <QNetworkAccessManager>
#include <QTest>

#include "akashi/network_service.h"
#include "akashi/service_registry.h"

namespace tests {
namespace unittests {

using namespace akashi;

class tst_NetworkService : public QObject
{
    Q_OBJECT

  private slots:
    void providesAManager();
    void resolvableThroughTheRegistry();
};

void tst_NetworkService::providesAManager()
{
    NetworkService l_service;
    QVERIFY(l_service.networkManager() != nullptr);
    // The same shared manager is returned each time.
    QCOMPARE(l_service.networkManager(), l_service.networkManager());
}

void tst_NetworkService::resolvableThroughTheRegistry()
{
    ServiceRegistry l_registry;
    l_registry.registerService(std::make_shared<NetworkService>());

    const std::shared_ptr<NetworkService> l_service = l_registry.resolve<NetworkService>("akashi.network");
    QVERIFY(l_service != nullptr);
    QVERIFY(l_service->networkManager() != nullptr);
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_NetworkService)

#include "tst_networkservice.moc"
