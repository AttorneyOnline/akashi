// AI-generated: written by Claude.
#include "core/websocket_receiver.h"

#include <QNetworkRequest>
#include <QTest>

namespace tests {
namespace unittests {

using namespace akashi;

class tst_WebSocketTransport : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void parseTrustedProxiesReadsIpsAndSubnets();
    void parseTrustedProxiesSkipsGarbage();
    void localhostIsAlwaysTrusted();
    void configuredSubnetIsTrusted();
    void untrustedPeerIgnoresProxyHeaders();
    void trustedProxyUsesXRealIp();
    void trustedProxyTakesTheLastForwardedHop();
    void xRealIpWinsOverForwardedFor();
    void malformedHeaderFallsBackToPeer();
    void capabilityTokensReadFromTheOfferList();
};

void tst_WebSocketTransport::parseTrustedProxiesReadsIpsAndSubnets()
{
    const TrustedProxyList l_list = WebSocketTransport::parseTrustedProxies("10.0.0.0/8, 192.168.1.5 , 172.16.0.0/12");
    QCOMPARE(l_list.size(), 3);
    // A bare IP is a host route.
    QCOMPARE(l_list.at(1).first, QHostAddress("192.168.1.5"));
    QCOMPARE(l_list.at(1).second, 32);
}

void tst_WebSocketTransport::parseTrustedProxiesSkipsGarbage()
{
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Ignoring invalid trusted proxy entry"));
    const TrustedProxyList l_list = WebSocketTransport::parseTrustedProxies("10.0.0.0/8, not-an-ip, ");
    QCOMPARE(l_list.size(), 1);
    QVERIFY(WebSocketTransport::parseTrustedProxies("").isEmpty());
}

void tst_WebSocketTransport::localhostIsAlwaysTrusted()
{
    // Even with no configured proxies, every loopback form is trusted.
    const TrustedProxyList l_none;
    QVERIFY(WebSocketTransport::isTrustedProxy(QHostAddress("127.0.0.1"), l_none));
    QVERIFY(WebSocketTransport::isTrustedProxy(QHostAddress("::1"), l_none));
    QVERIFY(WebSocketTransport::isTrustedProxy(QHostAddress("::ffff:127.0.0.1"), l_none));
    // A non-local peer with no configured proxies is not trusted.
    QVERIFY(!WebSocketTransport::isTrustedProxy(QHostAddress("203.0.113.9"), l_none));
}

void tst_WebSocketTransport::configuredSubnetIsTrusted()
{
    const TrustedProxyList l_list = WebSocketTransport::parseTrustedProxies("10.0.0.0/8");
    QVERIFY(WebSocketTransport::isTrustedProxy(QHostAddress("10.1.2.3"), l_list));
    QVERIFY(!WebSocketTransport::isTrustedProxy(QHostAddress("11.0.0.1"), l_list));
}

void tst_WebSocketTransport::untrustedPeerIgnoresProxyHeaders()
{
    // A direct internet client sends spoofed headers; they must be ignored
    // and its real socket address used instead.
    QNetworkRequest l_request;
    l_request.setRawHeader("x-forwarded-for", "1.2.3.4");
    l_request.setRawHeader("x-real-ip", "1.2.3.4");
    const QHostAddress l_peer("203.0.113.9");
    QCOMPARE(WebSocketTransport::resolveClientAddress(false, l_request, l_peer), l_peer);
}

void tst_WebSocketTransport::trustedProxyUsesXRealIp()
{
    QNetworkRequest l_request;
    l_request.setRawHeader("x-real-ip", "198.51.100.7");
    QCOMPARE(WebSocketTransport::resolveClientAddress(true, l_request, QHostAddress("127.0.0.1")),
             QHostAddress("198.51.100.7"));
}

// The core of the fix: the real client is the entry the trusted proxy
// appended (the last), not the first, which a client can prepend to spoof.
void tst_WebSocketTransport::trustedProxyTakesTheLastForwardedHop()
{
    QNetworkRequest l_request;
    // Client-spoofed 1.2.3.4, then the real peer the proxy saw.
    l_request.setRawHeader("x-forwarded-for", "1.2.3.4, 198.51.100.7");
    QCOMPARE(WebSocketTransport::resolveClientAddress(true, l_request, QHostAddress("127.0.0.1")),
             QHostAddress("198.51.100.7"));
}

void tst_WebSocketTransport::xRealIpWinsOverForwardedFor()
{
    QNetworkRequest l_request;
    l_request.setRawHeader("x-real-ip", "198.51.100.7");
    l_request.setRawHeader("x-forwarded-for", "203.0.113.1");
    QCOMPARE(WebSocketTransport::resolveClientAddress(true, l_request, QHostAddress("127.0.0.1")),
             QHostAddress("198.51.100.7"));
}

void tst_WebSocketTransport::malformedHeaderFallsBackToPeer()
{
    // A trusted proxy but an unparseable header: fall back to the socket peer
    // rather than a null address.
    QNetworkRequest l_request;
    l_request.setRawHeader("x-real-ip", "garbage");
    l_request.setRawHeader("x-forwarded-for", "also-garbage");
    const QHostAddress l_peer("127.0.0.1");
    QCOMPARE(WebSocketTransport::resolveClientAddress(true, l_request, l_peer), l_peer);
}

void tst_WebSocketTransport::capabilityTokensReadFromTheOfferList()
{
    // The comma-delimited offer list is the client's feature list: the
    // network_ namespace is stripped to the FL feature name, an assembled
    // packet key stays whole, and foreign tokens stay foreign.
    QNetworkRequest l_request;
    l_request.setRawHeader("Sec-WebSocket-Protocol",
                           "ao2.legacy, network_auth_password,ao_ms_2.11.1 , network_, chat.v2");
    QCOMPARE(WebSocketTransport::parseCapabilityTokens(l_request),
             QStringList({"auth_password", "ao_ms_2.11.1"}));

    // No offer at all reads as no announced features.
    QCOMPARE(WebSocketTransport::parseCapabilityTokens(QNetworkRequest()), QStringList());
}

} // namespace unittests
} // namespace tests

QTEST_GUILESS_MAIN(tests::unittests::tst_WebSocketTransport)
#include "tst_websocket_transport.moc"
