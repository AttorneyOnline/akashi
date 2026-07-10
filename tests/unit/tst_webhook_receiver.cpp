// AI-generated: written by Claude.
#include "testtools/webhook_receiver.h"

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSignalSpy>
#include <QTest>

namespace tests {
namespace unittests {

using akashi::WebhookReceiver;
using akashi::WebhookRequest;

class tst_WebhookReceiver : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void startPicksAFreePort();
    void capturesAPostVerbatim();
    void capturesRootAndGet();
    void answersWithTheConfiguredStatus();
    void takeRequestPopsOldestFirst();
    void jsonReadsOnlyObjectBodies();
};

// Posts a body with a content type, the way webhook senders do.
static QNetworkReply *post(QNetworkAccessManager &f_network, const QUrl &f_url, const QByteArray &f_body,
                           const QString &f_content_type = QStringLiteral("application/json"))
{
    QNetworkRequest l_request(f_url);
    l_request.setHeader(QNetworkRequest::ContentTypeHeader, f_content_type);
    return f_network.post(l_request, f_body);
}

void tst_WebhookReceiver::startPicksAFreePort()
{
    WebhookReceiver l_first;
    WebhookReceiver l_second;
    QVERIFY(l_first.start());
    QVERIFY(l_second.start());

    QVERIFY(l_first.port() != 0);
    QVERIFY(l_second.port() != 0);
    QVERIFY(l_first.port() != l_second.port());

    // A second start is harmless and keeps the port.
    const quint16 l_port = l_first.port();
    QVERIFY(l_first.start());
    QCOMPARE(l_first.port(), l_port);

    QCOMPARE(l_first.url(), QUrl(QStringLiteral("http://127.0.0.1:%1/webhook").arg(l_port)));
    QCOMPARE(l_first.url(QStringLiteral("no/slash")), QUrl(QStringLiteral("http://127.0.0.1:%1/no/slash").arg(l_port)));
}

void tst_WebhookReceiver::capturesAPostVerbatim()
{
    WebhookReceiver l_receiver;
    QVERIFY(l_receiver.start());
    QSignalSpy l_received(&l_receiver, &WebhookReceiver::requestReceived);

    QNetworkAccessManager l_network;
    QUrl l_url = l_receiver.url(QStringLiteral("/api/webhooks/1/token"));
    l_url.setQuery(QStringLiteral("wait=true"));
    QNetworkRequest l_request(l_url);
    l_request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    l_request.setRawHeader("X-Test-Marker", "present");
    QNetworkReply *l_reply = l_network.post(l_request, QByteArrayLiteral("{\"content\":\"Modcall!\"}"));

    QVERIFY(l_receiver.waitForRequests(1));
    QCOMPARE(l_received.count(), 1);
    QCOMPARE(l_receiver.requestCount(), 1);

    const WebhookRequest &l_captured = l_receiver.requestAt(0);
    QCOMPARE(l_captured.method, QStringLiteral("POST"));
    QCOMPARE(l_captured.path, QStringLiteral("/api/webhooks/1/token"));
    QCOMPARE(l_captured.query, QStringLiteral("wait=true"));
    QCOMPARE(l_captured.body, QByteArrayLiteral("{\"content\":\"Modcall!\"}"));
    QVERIFY(l_captured.header("content-type").startsWith("application/json"));
    QCOMPARE(l_captured.header("x-test-marker"), QByteArrayLiteral("present"));
    QCOMPARE(l_captured.header("absent"), QByteArray());

    const auto l_json = l_captured.json();
    QVERIFY(l_json.has_value());
    QCOMPARE(l_json->value(QStringLiteral("content")).toString(), QStringLiteral("Modcall!"));

    // The sender sees the default 204, like a happy Discord.
    QTRY_VERIFY(l_reply->isFinished());
    QCOMPARE(l_reply->error(), QNetworkReply::NoError);
    QCOMPARE(l_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 204);
    l_reply->deleteLater();
}

void tst_WebhookReceiver::capturesRootAndGet()
{
    WebhookReceiver l_receiver;
    QVERIFY(l_receiver.start());

    QNetworkAccessManager l_network;
    QNetworkReply *l_reply = l_network.get(QNetworkRequest(l_receiver.url(QStringLiteral("/"))));

    QVERIFY(l_receiver.waitForRequests(1));
    const WebhookRequest &l_captured = l_receiver.requestAt(0);
    QCOMPARE(l_captured.method, QStringLiteral("GET"));
    QCOMPARE(l_captured.path, QStringLiteral("/"));
    QVERIFY(l_captured.body.isEmpty());
    l_reply->deleteLater();
}

void tst_WebhookReceiver::answersWithTheConfiguredStatus()
{
    WebhookReceiver l_receiver;
    QVERIFY(l_receiver.start());
    l_receiver.setResponseStatus(429);

    QNetworkAccessManager l_network;
    QNetworkReply *l_reply = post(l_network, l_receiver.url(), QByteArrayLiteral("{}"));

    QTRY_VERIFY(l_reply->isFinished());
    QCOMPARE(l_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 429);
    QVERIFY(l_reply->error() != QNetworkReply::NoError);
    l_reply->deleteLater();
}

void tst_WebhookReceiver::takeRequestPopsOldestFirst()
{
    WebhookReceiver l_receiver;
    QVERIFY(l_receiver.start());

    // The replies are children of the manager; it cleans them up.
    QNetworkAccessManager l_network;
    post(l_network, l_receiver.url(), QByteArrayLiteral("one"), QStringLiteral("text/plain"));
    QVERIFY(l_receiver.waitForRequests(1));
    post(l_network, l_receiver.url(), QByteArrayLiteral("two"), QStringLiteral("text/plain"));
    QVERIFY(l_receiver.waitForRequests(2));

    QCOMPARE(l_receiver.takeRequest()->body, QByteArrayLiteral("one"));
    QCOMPARE(l_receiver.takeRequest()->body, QByteArrayLiteral("two"));
    QVERIFY(!l_receiver.takeRequest().has_value());

    post(l_network, l_receiver.url(), QByteArrayLiteral("three"), QStringLiteral("text/plain"));
    QVERIFY(l_receiver.waitForRequests(1));
    l_receiver.clear();
    QCOMPARE(l_receiver.requestCount(), 0);
}

void tst_WebhookReceiver::jsonReadsOnlyObjectBodies()
{
    WebhookRequest l_request;
    l_request.body = QByteArrayLiteral("{\"a\":1}");
    QVERIFY(l_request.json().has_value());

    l_request.body = QByteArrayLiteral("[1,2]");
    QVERIFY(!l_request.json().has_value());

    l_request.body = QByteArrayLiteral("not json");
    QVERIFY(!l_request.json().has_value());
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_WebhookReceiver)

#include "tst_webhook_receiver.moc"
