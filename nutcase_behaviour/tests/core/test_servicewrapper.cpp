#include <QTest>
#include <QObject>
#include <serviceregistry.h>
#include <servicewrapper.h>

class DummyObject : public QObject
{
    Q_OBJECT
public:
    explicit DummyObject(QObject *parent = nullptr) : QObject(parent), m_value(0) {}

    int value() const { return m_value; }
    void setValue(int v) { m_value = v; }

private:
    int m_value;
};

class TestServiceWrapper : public QObject
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

    void test_wrapAndRetrieve()
    {
        auto *dummy = new DummyObject(this);
        dummy->setValue(42);

        auto *wrapper = new ServiceWrapper<DummyObject>("TestAuthor",
                                                        "1.0.0",
                                                        "test.dummy",
                                                        dummy,
                                                        m_registry,
                                                        this);

        QVERIFY(m_registry->hasService("test.dummy"));

        DummyObject *retrieved = wrapper->instance();
        QVERIFY(retrieved != nullptr);
        QCOMPARE(retrieved->value(), 42);
        QCOMPARE(retrieved, dummy);
    }

    void test_serviceProperties()
    {
        auto *dummy = new DummyObject(this);

        auto *wrapper = new ServiceWrapper<DummyObject>("MyAuthor",
                                                        "2.0.0",
                                                        "test.properties",
                                                        dummy,
                                                        m_registry,
                                                        this);

        auto author = m_registry->getServiceInfo("test.properties", "author");
        auto version = m_registry->getServiceInfo("test.properties", "version");
        auto identifier = m_registry->getServiceInfo("test.properties", "identifier");

        QVERIFY(author.has_value());
        QVERIFY(version.has_value());
        QVERIFY(identifier.has_value());

        QCOMPARE(author.value(), QString("MyAuthor"));
        QCOMPARE(version.value(), QString("2.0.0"));
        QCOMPARE(identifier.value(), QString("test.properties"));
    }

    void test_retrieveFromRegistry()
    {
        auto *dummy = new DummyObject(this);
        dummy->setValue(999);

        new ServiceWrapper<DummyObject>("Author",
                                        "1.0",
                                        "test.retrieve",
                                        dummy,
                                        m_registry,
                                        this);

        auto *wrapper = m_registry->getService<ServiceWrapper<DummyObject>>("test.retrieve");

        QVERIFY(wrapper != nullptr);
        QVERIFY(wrapper->instance() != nullptr);
        QCOMPARE(wrapper->instance()->value(), 999);
    }

    void test_modifyWrappedObject()
    {
        auto *dummy = new DummyObject(this);
        dummy->setValue(100);

        auto *wrapper = new ServiceWrapper<DummyObject>("Author",
                                                        "1.0",
                                                        "test.modify",
                                                        dummy,
                                                        m_registry,
                                                        this);

        // Modify through wrapper
        wrapper->instance()->setValue(200);

        // Verify change
        QCOMPARE(dummy->value(), 200);
        QCOMPARE(wrapper->instance()->value(), 200);
    }

    void test_multipleWrappedServices()
    {
        auto *dummy1 = new DummyObject(this);
        auto *dummy2 = new DummyObject(this);

        dummy1->setValue(111);
        dummy2->setValue(222);

        new ServiceWrapper<DummyObject>("Author", "1.0", "test.wrapper1", dummy1, m_registry, this);
        new ServiceWrapper<DummyObject>("Author", "1.0", "test.wrapper2", dummy2, m_registry, this);

        auto *wrapper1 = m_registry->getService<ServiceWrapper<DummyObject>>("test.wrapper1");
        auto *wrapper2 = m_registry->getService<ServiceWrapper<DummyObject>>("test.wrapper2");

        QVERIFY(wrapper1 != nullptr);
        QVERIFY(wrapper2 != nullptr);
        QCOMPARE(wrapper1->instance()->value(), 111);
        QCOMPARE(wrapper2->instance()->value(), 222);
    }
};

QTEST_MAIN(TestServiceWrapper)
#include "test_servicewrapper.moc"
