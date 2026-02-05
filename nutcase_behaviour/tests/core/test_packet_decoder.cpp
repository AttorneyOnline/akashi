#include <QTest>
#include <packet.h>
#include <packet_decoder.h>

class TestPacketDecoder : public QObject
{
    Q_OBJECT

private slots:
    void test_fromListBasic()
    {
        Packet packet;
        QStringList data = {"123", "alice", "hello world"};

        PacketDecoder(packet)
            .fromList(data)
            .field<int>("id", 0, -1)
            .field<QString>("name", 1, QString(""))
            .field<QString>("message", 2, QString(""));

        QCOMPARE(packet.get<int>("id"), 123);
        QCOMPARE(packet.get<QString>("name"), QString("alice"));
        QCOMPARE(packet.get<QString>("message"), QString("hello world"));
    }

    void test_fromListWithDefaults()
    {
        Packet packet;
        QStringList data = {"42"};

        PacketDecoder(packet)
            .fromList(data)
            .field<int>("id", 0, -1)
            .field<QString>("name", 1, QString("default_name"))
            .field<int>("count", 2, 100);

        QCOMPARE(packet.get<int>("id"), 42);
        QCOMPARE(packet.get<QString>("name"), QString("default_name"));
        QCOMPARE(packet.get<int>("count"), 100);
    }

    void test_fromJsonBasic()
    {
        Packet packet;
        QJsonObject json;
        json["id"] = 123;
        json["name"] = "alice";
        json["active"] = true;

        PacketDecoder(packet)
            .fromJson(json)
            .field<int>("id", -1)
            .field<QString>("name", QString(""))
            .field<bool>("active", false);

        QCOMPARE(packet.get<int>("id"), 123);
        QCOMPARE(packet.get<QString>("name"), QString("alice"));
        QCOMPARE(packet.get<bool>("active"), true);
    }

    void test_fromJsonWithDefaults()
    {
        Packet packet;
        QJsonObject json;
        json["id"] = 42;

        PacketDecoder(packet)
            .fromJson(json)
            .field<int>("id", -1)
            .field<QString>("name", QString("default_name"))
            .field<int>("count", 100);

        QCOMPARE(packet.get<int>("id"), 42);
        QCOMPARE(packet.get<QString>("name"), QString("default_name"));
        QCOMPARE(packet.get<int>("count"), 100);
    }

    void test_fluentInterface()
    {
        Packet packet;
        QStringList data = {"1", "2", "3", "4", "5"};

        // Test that chaining works
        PacketDecoder decoder(packet);
        decoder.fromList(data)
            .field<int>("a", 0, 0)
            .field<int>("b", 1, 0)
            .field<int>("c", 2, 0)
            .field<int>("d", 3, 0)
            .field<int>("e", 4, 0);

        QCOMPARE(packet.get<int>("a"), 1);
        QCOMPARE(packet.get<int>("b"), 2);
        QCOMPARE(packet.get<int>("c"), 3);
        QCOMPARE(packet.get<int>("d"), 4);
        QCOMPARE(packet.get<int>("e"), 5);
    }

    void test_mixedTypes()
    {
        Packet packet;
        QJsonObject json;
        json["int_val"] = 42;
        json["string_val"] = "test";
        json["bool_val"] = true;
        json["double_val"] = 3.14;

        PacketDecoder(packet)
            .fromJson(json)
            .field<int>("int_val", 0)
            .field<QString>("string_val", QString(""))
            .field<bool>("bool_val", false)
            .field<double>("double_val", 0.0);

        QCOMPARE(packet.get<int>("int_val"), 42);
        QCOMPARE(packet.get<QString>("string_val"), QString("test"));
        QCOMPARE(packet.get<bool>("bool_val"), true);
        QCOMPARE(packet.get<double>("double_val"), 3.14);
    }

    void test_emptyData()
    {
        Packet packet;
        QStringList emptyList;

        PacketDecoder(packet)
            .fromList(emptyList)
            .field<int>("id", 0, -1)
            .field<QString>("name", 1, QString("default"));

        QCOMPARE(packet.get<int>("id"), -1);
        QCOMPARE(packet.get<QString>("name"), QString("default"));
    }
};

QTEST_MAIN(TestPacketDecoder)
#include "test_packet_decoder.moc"
