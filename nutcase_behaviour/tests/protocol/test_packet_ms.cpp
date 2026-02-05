#include <QTest>
#include <packet_ms.h>

class TestPacketMS : public QObject
{
    Q_OBJECT

private slots:
    void test_ServerMS_V26_FromList()
    {
        QStringList data = {
            "MS",                    // 0: header
            "1",                     // 1: desk_mod
            "preanim_test",          // 2: preanim
            "Phoenix",               // 3: character
            "normal",                // 4: emote
            "Test message",          // 5: message
            "wit",                   // 6: side
            "sfx-stab",              // 7: sfx_name
            "1",                     // 8: emote_modifier
            "5",                     // 9: char_id
            "500",                   // 10: sfx_delay
            "0",                     // 11: shout_modifier
            "1",                     // 12: evidence
            "0",                     // 13: flip
            "1",                     // 14: realization
            "0",                     // 15: text_color
            "Phoenix Wright",        // 16: showname
            "-1",                    // 17: other_charid
            "",                      // 18: other_name
            "",                      // 19: other_emote
            "0",                     // 20: self_offset
            "0"                      // 21: noninterrupting_preanim
        };

        ServerPacket::MS_V26 packet(PacketData{data});

        QCOMPARE(packet.header(), QString("MS"));
        QCOMPARE(packet.get<int>("desk_mod"), 1);
        QCOMPARE(packet.get<QString>("preanim"), QString("preanim_test"));
        QCOMPARE(packet.get<QString>("character"), QString("Phoenix"));
        QCOMPARE(packet.get<QString>("emote"), QString("normal"));
        QCOMPARE(packet.get<QString>("message"), QString("Test message"));
        QCOMPARE(packet.get<QString>("side"), QString("wit"));
        QCOMPARE(packet.get<QString>("sfx_name"), QString("sfx-stab"));
        QCOMPARE(packet.get<int>("emote_modifier"), 1);
        QCOMPARE(packet.get<int>("char_id"), 5);
        QCOMPARE(packet.get<int>("sfx_delay"), 500);
        QCOMPARE(packet.get<int>("shout_modifier"), 0);
        QCOMPARE(packet.get<int>("evidence"), 1);
        QCOMPARE(packet.get<int>("flip"), 0);
        QCOMPARE(packet.get<int>("realization"), 1);
        QCOMPARE(packet.get<int>("text_color"), 0);
        QCOMPARE(packet.get<QString>("showname"), QString("Phoenix Wright"));
        QCOMPARE(packet.get<int>("other_charid"), -1);
        QCOMPARE(packet.get<QString>("other_name"), QString(""));
        QCOMPARE(packet.get<QString>("other_emote"), QString(""));
        QCOMPARE(packet.get<QString>("self_offset"), QString("0"));
        QCOMPARE(packet.get<int>("noninterrupting_preanim"), 0);
    }

    void test_ServerMS_V26_FromJson()
    {
        QJsonObject json;
        json["desk_mod"] = 1;
        json["preanim"] = "preanim_test";
        json["character"] = "Edgeworth";
        json["emote"] = "thinking";
        json["message"] = "Objection!";
        json["side"] = "pro";
        json["char_id"] = 3;

        ServerPacket::MS_V26 packet(PacketData{json});

        QCOMPARE(packet.header(), QString("MS"));
        QCOMPARE(packet.get<int>("desk_mod"), 1);
        QCOMPARE(packet.get<QString>("preanim"), QString("preanim_test"));
        QCOMPARE(packet.get<QString>("character"), QString("Edgeworth"));
        QCOMPARE(packet.get<QString>("emote"), QString("thinking"));
        QCOMPARE(packet.get<QString>("message"), QString("Objection!"));
        QCOMPARE(packet.get<QString>("side"), QString("pro"));
        QCOMPARE(packet.get<int>("char_id"), 3);
    }

    void test_ServerMS_V26_WithDefaults()
    {
        QStringList minimalData = {
            "MS",         // 0: header
            "0"           // 1: desk_mod
        };

        ServerPacket::MS_V26 packet(PacketData{minimalData});

        // Check defaults are applied
        QCOMPARE(packet.get<int>("desk_mod"), 0);
        QCOMPARE(packet.get<QString>("preanim"), QString(""));
        QCOMPARE(packet.get<QString>("character"), QString(""));
        QCOMPARE(packet.get<QString>("side"), QString("def"));
        QCOMPARE(packet.get<int>("char_id"), -1);
        QCOMPARE(packet.get<int>("flip"), 0);
    }

    void test_ClientMS_V26_FromList()
    {
        QStringList data = {
            "MS",                    // 0: header
            "0",                     // 1: desk_mod
            "",                      // 2: preanim
            "Maya",                  // 3: character
            "happy",                 // 4: emote
            "Hello!",                // 5: message
            "def",                   // 6: side
            "",                      // 7: sfx_name
            "0",                     // 8: emote_modifier
            "2",                     // 9: char_id
            "0",                     // 10: sfx_delay
            "0",                     // 11: shout_modifier
            "0",                     // 12: evidence
            "0",                     // 13: flip
            "0",                     // 14: realization
            "0",                     // 15: text_color
            "Maya Fey",              // 16: showname
            "-1",                    // 17: other_charid
            "0",                     // 18: self_offset
            "0"                      // 19: noninterrupting_preanim
        };

        ClientPacket::MS_V26 packet(PacketData{data});

        QCOMPARE(packet.header(), QString("MS"));
        QCOMPARE(packet.get<QString>("character"), QString("Maya"));
        QCOMPARE(packet.get<QString>("emote"), QString("happy"));
        QCOMPARE(packet.get<QString>("message"), QString("Hello!"));
        QCOMPARE(packet.get<int>("char_id"), 2);
        QCOMPARE(packet.get<QString>("showname"), QString("Maya Fey"));
    }

    void test_ClientMS_V26_FromJson()
    {
        QJsonObject json;
        json["character"] = "Mia";
        json["emote"] = "normal";
        json["message"] = "Let me tell you something.";
        json["char_id"] = 4;
        json["showname"] = "Mia Fey";

        ClientPacket::MS_V26 packet(PacketData{json});

        QCOMPARE(packet.get<QString>("character"), QString("Mia"));
        QCOMPARE(packet.get<QString>("emote"), QString("normal"));
        QCOMPARE(packet.get<QString>("message"), QString("Let me tell you something."));
        QCOMPARE(packet.get<int>("char_id"), 4);
        QCOMPARE(packet.get<QString>("showname"), QString("Mia Fey"));
    }

    void test_PacketDataVariantHandling()
    {
        // Test that both QStringList and QJsonObject work
        QStringList listData = {"MS", "1", "test"};
        QJsonObject jsonData;
        jsonData["desk_mod"] = 1;
        jsonData["preanim"] = "test";

        ServerPacket::MS_V26 packet1(PacketData{listData});
        ServerPacket::MS_V26 packet2(PacketData{jsonData});

        QCOMPARE(packet1.header(), QString("MS"));
        QCOMPARE(packet2.header(), QString("MS"));

        QCOMPARE(packet1.get<int>("desk_mod"), 1);
        QCOMPARE(packet2.get<int>("desk_mod"), 1);

        QCOMPARE(packet1.get<QString>("preanim"), QString("test"));
        QCOMPARE(packet2.get<QString>("preanim"), QString("test"));
    }
};

QTEST_MAIN(TestPacketMS)
#include "test_packet_ms.moc"
