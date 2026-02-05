#include <QTest>
#include <packet_helpers.h>

class TestPacketHelpers : public QObject
{
    Q_OBJECT

private slots:
    void test_fromStringListInt()
    {
        QStringList list = {"42", "100", "999"};

        int result1 = PacketHelpers::fromStringList<int>(list, 0, -1);
        QCOMPARE(result1, 42);

        int result2 = PacketHelpers::fromStringList<int>(list, 1, -1);
        QCOMPARE(result2, 100);

        int result3 = PacketHelpers::fromStringList<int>(list, 2, -1);
        QCOMPARE(result3, 999);
    }

    void test_fromStringListIntDefault()
    {
        QStringList list = {"42"};

        int result = PacketHelpers::fromStringList<int>(list, 10, -1);
        QCOMPARE(result, -1); // Should return default
    }

    void test_fromStringListQString()
    {
        QStringList list = {"hello", "world", "test"};

        QString result1 = PacketHelpers::fromStringList<QString>(list, 0, QString("default"));
        QCOMPARE(result1, QString("hello"));

        QString result2 = PacketHelpers::fromStringList<QString>(list, 1, QString("default"));
        QCOMPARE(result2, QString("world"));
    }

    void test_fromStringListQStringDefault()
    {
        QStringList list = {"hello"};

        QString result = PacketHelpers::fromStringList<QString>(list, 10, QString("default"));
        QCOMPARE(result, QString("default"));
    }

    void test_fromJsonInt()
    {
        QJsonObject json;
        json["count"] = 42;
        json["value"] = 100;

        int result1 = PacketHelpers::fromJson<int>("count", json, -1);
        QCOMPARE(result1, 42);

        int result2 = PacketHelpers::fromJson<int>("value", json, -1);
        QCOMPARE(result2, 100);
    }

    void test_fromJsonIntDefault()
    {
        QJsonObject json;
        json["count"] = 42;

        int result = PacketHelpers::fromJson<int>("missing", json, -1);
        QCOMPARE(result, -1); // Should return default
    }

    void test_fromJsonQString()
    {
        QJsonObject json;
        json["name"] = "alice";
        json["message"] = "hello";

        QString result1 = PacketHelpers::fromJson<QString>("name", json, QString("default"));
        QCOMPARE(result1, QString("alice"));

        QString result2 = PacketHelpers::fromJson<QString>("message", json, QString("default"));
        QCOMPARE(result2, QString("hello"));
    }

    void test_fromJsonQStringDefault()
    {
        QJsonObject json;
        json["name"] = "alice";

        QString result = PacketHelpers::fromJson<QString>("missing", json, QString("default"));
        QCOMPARE(result, QString("default"));
    }

    void test_emptyList()
    {
        QStringList emptyList;

        int result = PacketHelpers::fromStringList<int>(emptyList, 0, -1);
        QCOMPARE(result, -1);
    }

    void test_emptyJson()
    {
        QJsonObject emptyJson;

        QString result = PacketHelpers::fromJson<QString>("any", emptyJson, QString("default"));
        QCOMPARE(result, QString("default"));
    }
};

QTEST_MAIN(TestPacketHelpers)
#include "test_packet_helpers.moc"
