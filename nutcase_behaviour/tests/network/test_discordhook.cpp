#include "discordhook.h"
#include "serviceregistry.h"
#include "servicewrapper.h"

#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QTest>

class TestDiscordHook : public QObject
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

    void test_createDiscordHook()
    {
        DiscordHook *hook = new DiscordHook(m_registry, this);

        QVERIFY(hook != nullptr);
        QVERIFY(m_registry->hasService("akashi.network.discordhook"));
    }

    void test_discordMessageBuilder()
    {
        DiscordMessage message;
        message.setRequestUrl("https://discord.com/api/webhooks/test")
            .setContent("Test message")
            .setUsername("TestBot");

        QJsonObject json = message.toJson();

        QVERIFY(json.contains("content"));
        QVERIFY(json.contains("username"));
        QCOMPARE(json["content"].toString(), QString("Test message"));
        QCOMPARE(json["username"].toString(), QString("TestBot"));
    }

    void test_discordMessageWithEmbed()
    {
        DiscordMessage message;
        message.setContent("Main content")
            .beginEmbed()
            .setEmbedTitle("Embed Title")
            .setEmbedDescription("Embed Description")
            .setEmbedColor(0xFF0000)
            .endEmbed();

        QJsonObject json = message.toJson();

        QVERIFY(json.contains("embeds"));
        QJsonArray embeds = json["embeds"].toArray();
        QCOMPARE(embeds.size(), 1);

        QJsonObject embed = embeds[0].toObject();
        QCOMPARE(embed["title"].toString(), QString("Embed Title"));
        QCOMPARE(embed["description"].toString(), QString("Embed Description"));
        QCOMPARE(embed["color"].toInt(), 0xFF0000);
    }

    void test_discordMessageWithFields()
    {
        DiscordMessage message;
        message.beginEmbed()
            .setEmbedTitle("Test")
            .addEmbedField("Field1", "Value1", false)
            .addEmbedField("Field2", "Value2", true)
            .endEmbed();

        QJsonObject json = message.toJson();
        QJsonArray embeds = json["embeds"].toArray();
        QJsonObject embed = embeds[0].toObject();

        QVERIFY(embed.contains("fields"));
        QJsonArray fields = embed["fields"].toArray();
        QCOMPARE(fields.size(), 2);

        QJsonObject field1 = fields[0].toObject();
        QCOMPARE(field1["name"].toString(), QString("Field1"));
        QCOMPARE(field1["value"].toString(), QString("Value1"));
        QCOMPARE(field1["inline"].toBool(), false);

        QJsonObject field2 = fields[1].toObject();
        QCOMPARE(field2["name"].toString(), QString("Field2"));
        QCOMPARE(field2["value"].toString(), QString("Value2"));
        QCOMPARE(field2["inline"].toBool(), true);
    }

    void test_discordMultipartMessage()
    {
        DiscordMultipartMessage message;
        message.setRequestUrl("https://discord.com/api/webhooks/test");

        QJsonObject payload;
        payload["content"] = "Test with file";
        message.setPayloadJson(payload);

        message.addPart(QByteArray("file content"), "file", "test.txt");

        QCOMPARE(message.size(), 1);
        QCOMPARE(message.partAt(0).name, QString("file"));
        QCOMPARE(message.partAt(0).filename, QString("test.txt"));
        QCOMPARE(message.payloadJson()["content"].toString(), QString("Test with file"));
    }

    void test_messageChaining()
    {
        DiscordMessage message;

        auto &result = message.setContent("test")
                           .setUsername("bot")
                           .setAvatarUrl("https://example.com/avatar.png")
                           .setTts(false);

        // Verify chaining returns reference
        QCOMPARE(&result, &message);

        QJsonObject json = message.toJson();
        QCOMPARE(json["content"].toString(), QString("test"));
        QCOMPARE(json["username"].toString(), QString("bot"));
        QCOMPARE(json["avatar_url"].toString(), QString("https://example.com/avatar.png"));
        QCOMPARE(json["tts"].toBool(), false);
    }
};

QTEST_MAIN(TestDiscordHook)
#include "test_discordhook.moc"
