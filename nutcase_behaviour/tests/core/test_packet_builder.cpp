#include <QTest>
#include <packet.h>
#include <packet_builder.h>

class TestPacketBuilder : public QObject
{
    Q_OBJECT

private slots:
    void test_basicBuild()
    {
        auto packet = PacketBuilder()
                          .withHeader("TEST")
                          .withField<int>("id", 42)
                          .withField<QString>("name", "alice")
                          .build();

        QCOMPARE(packet->header(), QString("TEST"));
        QCOMPARE(packet->get<int>("id"), 42);
        QCOMPARE(packet->get<QString>("name"), QString("alice"));
    }

    void test_fluentInterface()
    {
        auto packet = PacketBuilder()
                          .withHeader("FLUENT")
                          .withField<int>("a", 1)
                          .withField<int>("b", 2)
                          .withField<int>("c", 3)
                          .withField<QString>("text", "test")
                          .build();

        QCOMPARE(packet->get<int>("a"), 1);
        QCOMPARE(packet->get<int>("b"), 2);
        QCOMPARE(packet->get<int>("c"), 3);
        QCOMPARE(packet->get<QString>("text"), QString("test"));
    }

    void test_conditionalBuilding()
    {
        bool includeOptional = true;
        bool excludeField = false;

        auto packet = PacketBuilder()
                          .withField<int>("always", 1)
                          .withFieldIf(includeOptional, "optional", 2)
                          .withFieldIf(excludeField, "excluded", 3)
                          .build();

        QVERIFY(packet->contains("always"));
        QVERIFY(packet->contains("optional"));
        QVERIFY(!packet->contains("excluded"));

        QCOMPARE(packet->get<int>("always"), 1);
        QCOMPARE(packet->get<int>("optional"), 2);
    }

    void test_buildRaw()
    {
        Packet *packet = PacketBuilder()
                             .withHeader("RAW")
                             .withField<int>("value", 100)
                             .buildRaw();

        QVERIFY(packet != nullptr);
        QCOMPARE(packet->header(), QString("RAW"));
        QCOMPARE(packet->get<int>("value"), 100);

        delete packet;
    }

    void test_buildAndEncode()
    {
        auto encoder = [](const QVariantMap &data) -> QByteArray
        {
            return QString("encoded:%1").arg(data.size()).toUtf8();
        };

        QByteArray encoded = PacketBuilder()
                                 .withField<int>("a", 1)
                                 .withField<int>("b", 2)
                                 .buildAndEncode(encoder);

        QVERIFY(!encoded.isEmpty());
        QVERIFY(encoded.contains("encoded:"));
    }

    void test_mixedTypes()
    {
        auto packet = PacketBuilder()
                          .withField<int>("int_val", 42)
                          .withField<QString>("string_val", "test")
                          .withField<double>("double_val", 3.14)
                          .withField<bool>("bool_val", true)
                          .build();

        QCOMPARE(packet->get<int>("int_val"), 42);
        QCOMPARE(packet->get<QString>("string_val"), QString("test"));
        QCOMPARE(packet->get<double>("double_val"), 3.14);
        QCOMPARE(packet->get<bool>("bool_val"), true);
    }

    void test_emptyPacket()
    {
        auto packet = PacketBuilder().build();

        QVERIFY(packet != nullptr);
        QVERIFY(packet->header().isEmpty());
    }
};

// Test derived packet type
class CustomPacket : public Packet
{
public:
    CustomPacket()
    {
        setHeader("CUSTOM");
    }

    int customValue() const { return get<int>("custom"); }
};

class TestTypedPacketBuilder : public QObject
{
    Q_OBJECT

private slots:
    void test_typedBuilder()
    {
        auto packet = TypedPacketBuilder<CustomPacket>()
                          .withField<int>("custom", 999)
                          .build();

        QCOMPARE(packet->header(), QString("CUSTOM"));
        QCOMPARE(packet->customValue(), 999);
    }

    void test_typedBuilderRaw()
    {
        CustomPacket *packet = TypedPacketBuilder<CustomPacket>()
                                   .withField<int>("custom", 777)
                                   .buildRaw();

        QVERIFY(packet != nullptr);
        QCOMPARE(packet->customValue(), 777);

        delete packet;
    }
};

QTEST_MAIN(TestPacketBuilder)
#include "test_packet_builder.moc"
