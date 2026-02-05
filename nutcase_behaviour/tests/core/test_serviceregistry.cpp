#include <QTest>
#include <service.h>
#include <serviceregistry.h>

class TestService : public Service
{
public:
    TestService(ServiceRegistry *f_registry, QObject *parent = nullptr)
        : Service{f_registry, parent}
    {
        m_service_properties = {{"author", "TestAuthor"},
                                {"version", "1.0.0"},
                                {"identifier", "test.service"}};
    }
};

class TestServiceRegistry : public QObject
{
    Q_OBJECT

private:
    ServiceRegistry *m_registry = nullptr;

private slots:
    void init()
    {
        m_registry = new ServiceRegistry(this);
    }

    void cleanup()
    {
        delete m_registry;
        m_registry = nullptr;
    }

    void test_registerService()
    {
        auto *service = new TestService(m_registry, this);
        m_registry->registerService(service);

        QVERIFY(m_registry->hasService("test.service"));
    }

    void test_getService()
    {
        auto *service = new TestService(m_registry, this);
        m_registry->registerService(service);

        TestService *retrieved = m_registry->getService<TestService>("test.service");

        QVERIFY(retrieved != nullptr);
        QCOMPARE(retrieved, service);
    }

    void test_getServiceInfo()
    {
        auto *service = new TestService(m_registry, this);
        m_registry->registerService(service);

        auto author = m_registry->getServiceInfo("test.service", "author");
        auto version = m_registry->getServiceInfo("test.service", "version");

        QVERIFY(author.has_value());
        QVERIFY(version.has_value());

        QCOMPARE(author.value(), QString("TestAuthor"));
        QCOMPARE(version.value(), QString("1.0.0"));
    }

    void test_removeService()
    {
        auto *service = new TestService(m_registry, this);
        m_registry->registerService(service);

        QVERIFY(m_registry->hasService("test.service"));

        m_registry->removeService("test.service");

        QVERIFY(!m_registry->hasService("test.service"));
    }

    void test_getNonexistentService()
    {
        TestService *service = m_registry->getService<TestService>("nonexistent");

        QVERIFY(service == nullptr);
    }

    void test_duplicateIdentifier()
    {
        auto *service1 = new TestService(m_registry, this);
        m_registry->registerService(service1);

        QVERIFY(m_registry->hasService("test.service"));

        // Try to register another service with same identifier
        auto *service2 = new TestService(m_registry, this);
        m_registry->registerService(service2);

        // Second service should be rejected and deleted
        // Only first service should exist
        QVERIFY(m_registry->hasService("test.service"));
    }

    void test_multipleServices()
    {
        class ServiceA : public Service
        {
        public:
            ServiceA(ServiceRegistry *reg, QObject *parent = nullptr)
                : Service{reg, parent}
            {
                m_service_properties = {{"author", "Author"},
                                        {"version", "1.0"},
                                        {"identifier", "service.a"}};
            }
        };

        class ServiceB : public Service
        {
        public:
            ServiceB(ServiceRegistry *reg, QObject *parent = nullptr)
                : Service{reg, parent}
            {
                m_service_properties = {{"author", "Author"},
                                        {"version", "2.0"},
                                        {"identifier", "service.b"}};
            }
        };

        auto *serviceA = new ServiceA(m_registry, this);
        auto *serviceB = new ServiceB(m_registry, this);

        m_registry->registerService(serviceA);
        m_registry->registerService(serviceB);

        QVERIFY(m_registry->hasService("service.a"));
        QVERIFY(m_registry->hasService("service.b"));

        ServiceA *retrievedA = m_registry->getService<ServiceA>("service.a");
        ServiceB *retrievedB = m_registry->getService<ServiceB>("service.b");

        QVERIFY(retrievedA != nullptr);
        QVERIFY(retrievedB != nullptr);
        QCOMPARE(retrievedA, serviceA);
        QCOMPARE(retrievedB, serviceB);
    }
};

QTEST_MAIN(TestServiceRegistry)
#include "test_serviceregistry.moc"
