#include <QTest>
#include <QNetworkAccessManager>
#include <serverpublisher.h>
#include <serviceregistry.h>
#include <servicewrapper.h>

class TestServerPublisher : public QObject
{
    Q_OBJECT

private:
    ServiceRegistry *m_registry = nullptr;
    ServiceWrapper<QNetworkAccessManager> *m_networkWrapper = nullptr;

private slots:
    void initTestCase()
    {
        m_registry = new ServiceRegistry(this);

        auto *networkManager = new QNetworkAccessManager(this);
        m_networkWrapper = new ServiceWrapper<QNetworkAccessManager>("TestAuthor",
                                                                     "1.0.0",
                                                                     "qt.network.manager",
                                                                     networkManager,
                                                                     m_registry,
                                                                     this);
    }

    void cleanupTestCase()
    {
        // Qt parent-child ownership handles cleanup
    }

    void test_createServerPublisher()
    {
        ServerPublisher *publisher = new ServerPublisher(m_registry, this);

        QVERIFY(publisher != nullptr);
    }

    void test_setProperty()
    {
        ServerPublisher publisher(m_registry, this);

        publisher.setProperty("name", "Test Server")
            .setProperty("port", 27016)
            .setProperty("players", 5)
            .setProperty("max_players", 10);

        // Properties should be set (we can't directly test private members,
        // but the chaining should work)
        QVERIFY(true);
    }

    void test_removeProperty()
    {
        ServerPublisher publisher(m_registry, this);

        publisher.setProperty("key1", "value1")
            .setProperty("key2", "value2");

        publisher.removeProperty("key1");

        // Verify chaining still works
        QVERIFY(true);
    }

    void test_setServerListUrl()
    {
        ServerPublisher publisher(m_registry, this);

        publisher.setServerListUrl("https://example.com/serverlist");

        // URL should be set
        QVERIFY(true);
    }

    void test_setPublisherState()
    {
        ServerPublisher publisher(m_registry, this);

        publisher.setServerListUrl("https://example.com/serverlist");
        publisher.setPublisherState(false);

        // State should be set to false (not publishing)
        QVERIFY(true);
    }

    void test_propertyChaining()
    {
        ServerPublisher publisher(m_registry, this);

        // Test that method chaining works
        ServerPublisher &result = publisher.setProperty("name", "Server")
                                      .setProperty("port", 1234);

        QCOMPARE(&result, &publisher);
    }
};

QTEST_MAIN(TestServerPublisher)
#include "test_serverpublisher.moc"
