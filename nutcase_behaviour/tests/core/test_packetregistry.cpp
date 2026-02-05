#include <QTest>
#include <packet.h>
#include <packet_decoder.h>
#include <packetregistry.h>
#include <serviceregistry.h>

class TestPacket : public Packet
{
public:
    explicit TestPacket(const PacketData &f_data)
    {
        setHeader("TEST");

        if (std::holds_alternative<QStringList>(f_data))
        {
            const auto &list = std::get<QStringList>(f_data);
            PacketDecoder(*this)
                .fromList(list)
                .field<int>("id", 0, -1)
                .field<QString>("name", 1, QString(""));
        }
        else
        {
            const auto &json = std::get<QJsonObject>(f_data);
            PacketDecoder(*this)
                .fromJson(json)
                .field<int>("id", -1)
                .field<QString>("name", QString(""));
        }
    }
};

class TestPacketRegistry : public QObject
{
    Q_OBJECT

private:
    ServiceRegistry *m_serviceRegistry = nullptr;
    PacketRegistry *m_packetRegistry = nullptr;

private slots:
    void initTestCase()
    {
        m_serviceRegistry = new ServiceRegistry(this);
        m_packetRegistry = new PacketRegistry(m_serviceRegistry, this);
    }

    void cleanupTestCase()
    {
        // Qt parent-child ownership handles cleanup
    }

    void test_registerAndCreateFromList()
    {
        m_packetRegistry->registerPacket("TEST", "1.0", "TP", [](const PacketData &data) -> Packet *
                                         { return new TestPacket(data); });

        QStringList data = {"42", "alice"};
        auto maybePacket = m_packetRegistry->create("TEST", "1.0", "TP", data);

        QVERIFY(maybePacket.has_value());

        Packet *packet = maybePacket.value();
        QVERIFY(packet != nullptr);
        QCOMPARE(packet->get<int>("id"), 42);
        QCOMPARE(packet->get<QString>("name"), QString("alice"));

        delete packet;
    }

    void test_registerAndCreateFromJson()
    {
        m_packetRegistry->registerPacket("TEST", "1.0", "TJ", [](const PacketData &data) -> Packet *
                                         { return new TestPacket(data); });

        QJsonObject json;
        json["id"] = 99;
        json["name"] = "bob";

        auto maybePacket = m_packetRegistry->create("TEST", "1.0", "TJ", json);

        QVERIFY(maybePacket.has_value());

        Packet *packet = maybePacket.value();
        QVERIFY(packet != nullptr);
        QCOMPARE(packet->get<int>("id"), 99);
        QCOMPARE(packet->get<QString>("name"), QString("bob"));

        delete packet;
    }

    void test_createNonexistent()
    {
        auto maybePacket = m_packetRegistry->create("INVALID", "1.0", "XX", QStringList());

        QVERIFY(!maybePacket.has_value());
    }

    void test_unregisterPacket()
    {
        m_packetRegistry->registerPacket("TEST", "1.0", "TR", [](const PacketData &data) -> Packet *
                                         { return new TestPacket(data); });

        // Verify it exists
        auto maybePacket1 = m_packetRegistry->create("TEST", "1.0", "TR", QStringList{"1", "test"});
        QVERIFY(maybePacket1.has_value());
        delete maybePacket1.value();

        // Unregister
        m_packetRegistry->unregisterPacket("TEST", "1.0", "TR");

        // Verify it's gone
        auto maybePacket2 = m_packetRegistry->create("TEST", "1.0", "TR", QStringList{"1", "test"});
        QVERIFY(!maybePacket2.has_value());
    }

    void test_multipleVersions()
    {
        // Register v1.0
        m_packetRegistry->registerPacket("MULTI", "1.0", "MV", [](const PacketData &data) -> Packet *
                                         {
            auto *packet = new Packet();
            packet->setHeader("MV_V1");
            return packet; });

        // Register v2.0
        m_packetRegistry->registerPacket("MULTI", "2.0", "MV", [](const PacketData &data) -> Packet *
                                         {
            auto *packet = new Packet();
            packet->setHeader("MV_V2");
            return packet; });

        auto packet1 = m_packetRegistry->create("MULTI", "1.0", "MV", QStringList());
        auto packet2 = m_packetRegistry->create("MULTI", "2.0", "MV", QStringList());

        QVERIFY(packet1.has_value());
        QVERIFY(packet2.has_value());

        QCOMPARE(packet1.value()->header(), QString("MV_V1"));
        QCOMPARE(packet2.value()->header(), QString("MV_V2"));

        delete packet1.value();
        delete packet2.value();
    }

    void test_differentArchitectures()
    {
        m_packetRegistry->registerPacket("ARCH1", "1.0", "DA", [](const PacketData &data) -> Packet *
                                         {
            auto *packet = new Packet();
            packet->set<QString>("arch", "ARCH1");
            return packet; });

        m_packetRegistry->registerPacket("ARCH2", "1.0", "DA", [](const PacketData &data) -> Packet *
                                         {
            auto *packet = new Packet();
            packet->set<QString>("arch", "ARCH2");
            return packet; });

        auto packet1 = m_packetRegistry->create("ARCH1", "1.0", "DA", QStringList());
        auto packet2 = m_packetRegistry->create("ARCH2", "1.0", "DA", QStringList());

        QVERIFY(packet1.has_value());
        QVERIFY(packet2.has_value());

        QCOMPARE(packet1.value()->get<QString>("arch"), QString("ARCH1"));
        QCOMPARE(packet2.value()->get<QString>("arch"), QString("ARCH2"));

        delete packet1.value();
        delete packet2.value();
    }
};

QTEST_MAIN(TestPacketRegistry)
#include "test_packetregistry.moc"
