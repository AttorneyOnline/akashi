#include <QTest>
#include <packet.h>

class TestPacket : public QObject
{
    Q_OBJECT

private slots:
    void test_setAndGet()
    {
        Packet packet;

        // Test int
        packet.set<int>("count", 42);
        QCOMPARE(packet.get<int>("count"), 42);

        // Test QString
        packet.set<QString>("name", "test");
        QCOMPARE(packet.get<QString>("name"), QString("test"));

        // Test double
        packet.set<double>("value", 3.14);
        QCOMPARE(packet.get<double>("value"), 3.14);

        // Test bool
        packet.set<bool>("flag", true);
        QCOMPARE(packet.get<bool>("flag"), true);
    }

    void test_contains()
    {
        Packet packet;
        packet.set<int>("id", 123);

        QVERIFY(packet.contains("id"));
        QVERIFY(!packet.contains("nonexistent"));
    }

    void test_header()
    {
        Packet packet;
        packet.setHeader("MS_TEST");

        QCOMPARE(packet.header(), QString("MS_TEST"));
    }

    void test_multipleFields()
    {
        Packet packet;
        packet.setHeader("MULTI");
        
        packet.set<int>("field1", 100);
        packet.set<QString>("field2", "data");
        packet.set<bool>("field3", false);
        packet.set<double>("field4", 2.718);

        QCOMPARE(packet.get<int>("field1"), 100);
        QCOMPARE(packet.get<QString>("field2"), QString("data"));
        QCOMPARE(packet.get<bool>("field3"), false);
        QCOMPARE(packet.get<double>("field4"), 2.718);
    }

    void test_overwrite()
    {
        Packet packet;
        packet.set<int>("value", 10);
        QCOMPARE(packet.get<int>("value"), 10);

        // Overwrite
        packet.set<int>("value", 20);
        QCOMPARE(packet.get<int>("value"), 20);
    }

    void test_encode()
    {
        Packet packet;
        packet.setHeader("TEST");
        packet.set<int>("value", 100);
        packet.set<QString>("text", "hello");

        auto encoder = [](const QVariantMap &data) -> QByteArray
        {
            return QString("encoded:%1").arg(data.size()).toUtf8();
        };

        QByteArray encoded = packet.encode(encoder);
        QVERIFY(!encoded.isEmpty());
        QVERIFY(encoded.contains("encoded:"));
    }

    void test_emptyPacket()
    {
        Packet packet;
        
        QVERIFY(packet.header().isEmpty());
        QVERIFY(!packet.contains("anything"));
    }
};

QTEST_MAIN(TestPacket)
#include "test_packet.moc"
