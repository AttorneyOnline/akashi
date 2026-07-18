// AI-generated: written by Claude.
#include "akashi/config_store.h"
#include "core/server_publisher.h"
#include "core/server_settings.h"
#include "testtools/webhook_receiver.h"

#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

namespace tests {
namespace unittests {

class tst_ServerPublisher : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void successfulAdvertiseSaysSo();
    void masterserverErrorReportReachesTheLog();
};

// Waits out the loopback round trip: the request arriving at the receiver,
// then the reply crossing back to the publisher on a later event-loop turn.
static void waitForReply(akashi::WebhookReceiver &f_receiver)
{
    QVERIFY(f_receiver.waitForRequests(1));
    QTest::qWait(500);
}

void tst_ServerPublisher::successfulAdvertiseSaysSo()
{
    akashi::WebhookReceiver l_receiver;
    QVERIFY(l_receiver.start());
    l_receiver.setResponse(QStringLiteral("/servers"), 200, QByteArrayLiteral("{}"));

    QTemporaryDir l_dir;
    akashi::ConfigStore l_store(l_dir.path());
    ServerSettings l_settings(&l_store);
    QVERIFY(l_settings.declare());
    l_settings.ms_ip.set(l_receiver.url(QStringLiteral("/servers")).toString());

    int l_players = 0;
    QTest::ignoreMessage(QtInfoMsg, QRegularExpression(QStringLiteral("Successfully advertised")));
    ServerPublisher l_publisher(27016, &l_players, &l_settings);
    waitForReply(l_receiver);
}

void tst_ServerPublisher::masterserverErrorReportReachesTheLog()
{
    // A refusal carries the masterserver's own error report in the body;
    // it must reach the log, not vanish behind the transport error.
    akashi::WebhookReceiver l_receiver;
    QVERIFY(l_receiver.start());
    l_receiver.setResponse(QStringLiteral("/servers"), 500,
                           QByteArrayLiteral(R"({"errors": [{"type": "invalid_port", "message": "port unreachable"}]})"));

    QTemporaryDir l_dir;
    akashi::ConfigStore l_store(l_dir.path());
    ServerSettings l_settings(&l_store);
    QVERIFY(l_settings.declare());
    l_settings.ms_ip.set(l_receiver.url(QStringLiteral("/servers")).toString());

    int l_players = 0;
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("Failed to advertise to the serverlist")));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("HTTP status code: 500")));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("invalid_port.*port unreachable")));
    ServerPublisher l_publisher(27016, &l_players, &l_settings);
    waitForReply(l_receiver);
}

} // namespace unittests
} // namespace tests

QTEST_GUILESS_MAIN(tests::unittests::tst_ServerPublisher)

#include "tst_server_publisher.moc"
