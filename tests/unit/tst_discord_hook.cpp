// AI-generated: written by Claude.
#include "core/discord_hook.h"
#include "core/discord_message.h"
#include "testtools/webhook_receiver.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTest>

namespace tests {
namespace unittests {

using namespace akashi;

class tst_DiscordHook : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void postDeliversTheJsonPayload();
    void postDeliversMultipartWithPayloadAndFile();
    void invalidUrlSendsNothing();
    void failureResponseIsLogged();
};

void tst_DiscordHook::postDeliversTheJsonPayload()
{
    WebhookReceiver l_receiver;
    QVERIFY(l_receiver.start());

    QNetworkAccessManager l_network;
    QSignalSpy l_finished(&l_network, &QNetworkAccessManager::finished);
    DiscordHook l_hook(&l_network);

    DiscordMessage l_message;
    l_message.setRequestUrl(l_receiver.url(QStringLiteral("/api/webhooks/1/token")).toString())
        .setUsername(QStringLiteral("akashi"))
        .setContent(QStringLiteral("Modcall in Courtroom"))
        .beginEmbed()
        .setEmbedTitle(QStringLiteral("Modcall"))
        .setEmbedDescription(QStringLiteral("Phoenix needs a mod"))
        .endEmbed();
    l_hook.post(l_message);

    QVERIFY(l_receiver.waitForRequests(1));
    const auto l_request = l_receiver.takeRequest();
    QVERIFY(l_request.has_value());
    QCOMPARE(l_request->method, QStringLiteral("POST"));
    QCOMPARE(l_request->path, QStringLiteral("/api/webhooks/1/token"));
    QVERIFY(l_request->header("content-type").startsWith("application/json"));

    const auto l_json = l_request->json();
    QVERIFY(l_json.has_value());
    QCOMPARE(l_json->value(QStringLiteral("username")).toString(), QStringLiteral("akashi"));
    QCOMPARE(l_json->value(QStringLiteral("content")).toString(), QStringLiteral("Modcall in Courtroom"));
    const QJsonArray l_embeds = l_json->value(QStringLiteral("embeds")).toArray();
    QCOMPARE(l_embeds.size(), 1);
    QCOMPARE(l_embeds.first().toObject().value(QStringLiteral("title")).toString(), QStringLiteral("Modcall"));

    // The 204 comes back and the hook has nothing to complain about.
    QTRY_COMPARE(l_finished.size(), 1);
}

void tst_DiscordHook::postDeliversMultipartWithPayloadAndFile()
{
    WebhookReceiver l_receiver;
    QVERIFY(l_receiver.start());

    QNetworkAccessManager l_network;
    QSignalSpy l_finished(&l_network, &QNetworkAccessManager::finished);
    DiscordHook l_hook(&l_network);

    DiscordMultipartMessage l_message;
    l_message.setRequestUrl(l_receiver.url(QStringLiteral("/api/webhooks/1/token")).toString());
    l_message.addPart(QByteArrayLiteral("day 1 evidence log"), QStringLiteral("files[0]"),
                      QStringLiteral("log.txt"), QStringLiteral("text/plain"), QStringLiteral("utf-8"));
    l_message.setPayloadJson(QJsonObject{{QStringLiteral("content"), QStringLiteral("Ban issued")}});
    l_hook.post(l_message);

    QVERIFY(l_receiver.waitForRequests(1));
    const auto l_request = l_receiver.takeRequest();
    QVERIFY(l_request.has_value());
    QCOMPARE(l_request->method, QStringLiteral("POST"));
    QVERIFY(l_request->header("content-type").startsWith("multipart/form-data"));

    // The raw multipart body carries the file part and the payload part.
    QVERIFY(l_request->body.contains("name=\"files[0]\""));
    QVERIFY(l_request->body.contains("filename=\"log.txt\""));
    QVERIFY(l_request->body.contains("text/plain; charset=utf-8"));
    QVERIFY(l_request->body.contains("day 1 evidence log"));
    QVERIFY(l_request->body.contains("name=\"payload_json\""));
    QVERIFY(l_request->body.contains("Ban issued"));

    QTRY_COMPARE(l_finished.size(), 1);
}

void tst_DiscordHook::invalidUrlSendsNothing()
{
    QNetworkAccessManager l_network;
    DiscordHook l_hook(&l_network);

    QTest::ignoreMessage(QtWarningMsg, "Cannot post: invalid webhook URL");
    l_hook.post(DiscordMessage());
    QTest::ignoreMessage(QtWarningMsg, "Cannot post: invalid webhook URL");
    l_hook.post(DiscordMultipartMessage());
}

void tst_DiscordHook::failureResponseIsLogged()
{
    WebhookReceiver l_receiver;
    QVERIFY(l_receiver.start());
    l_receiver.setResponseStatus(429);

    QNetworkAccessManager l_network;
    QSignalSpy l_finished(&l_network, &QNetworkAccessManager::finished);
    DiscordHook l_hook(&l_network);

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("Webhook failed:")));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("Response body:")));

    DiscordMessage l_message;
    l_message.setRequestUrl(l_receiver.url().toString()).setContent(QStringLiteral("hello"));
    l_hook.post(l_message);

    QTRY_COMPARE(l_finished.size(), 1);
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_DiscordHook)

#include "tst_discord_hook.moc"
