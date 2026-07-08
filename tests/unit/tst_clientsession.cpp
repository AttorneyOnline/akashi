// AI-generated: written by Claude.
#include "core/client_session.h"
#include "core/player_state.h"
#include "fake_transport.h"
#include "proto/packet.h"

#include <QSignalSpy>
#include <QTest>

class tst_ClientSession : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void writesThroughOpenTransport();
    void buffersWhileConnectionIsDown();
    void rebindReplaysPendingInOrder();
    void bufferIsBoundedAndRecordsOverflow();
    void rebindReplacesAndDeletesOldTransport();
    void forwardsTransportSignals();
    void addsOnePlayerAndEnforcesTheCap();
    void reportsTimeoutWhenNobodyReconnects();
    void bindingANewTransportCancelsTheWait();
};

void tst_ClientSession::writesThroughOpenTransport()
{
    FakeTransport *l_transport = new FakeTransport(true);
    akashi::ClientSession l_session(nullptr, l_transport, 1);

    l_session.write(akashi::Packet("CT", {"server", "hello"}));

    QCOMPARE(l_transport->written.size(), 1);
    QCOMPARE(l_session.pending_packets.size(), 0);
}

void tst_ClientSession::buffersWhileConnectionIsDown()
{
    FakeTransport *l_transport = new FakeTransport(true);
    akashi::ClientSession l_session(nullptr, l_transport, 1);
    l_transport->close();

    l_session.write(akashi::Packet("CT", {"server", "missed you"}));

    QCOMPARE(l_transport->written.size(), 0);
    QCOMPARE(l_session.pending_packets.size(), 1);
}

void tst_ClientSession::rebindReplaysPendingInOrder()
{
    FakeTransport *l_transport = new FakeTransport(true);
    akashi::ClientSession l_session(nullptr, l_transport, 1);
    l_transport->close();

    l_session.write(akashi::Packet("CT", {"server", "first"}));
    l_session.write(akashi::Packet("CT", {"server", "second"}));

    FakeTransport *l_replacement = new FakeTransport(true);
    l_session.bindTransport(l_replacement);

    QCOMPARE(l_session.pending_packets.size(), 0);
    QCOMPARE(l_replacement->written.size(), 2);
    QCOMPARE(l_replacement->written[0].fields()[1], QString("first"));
    QCOMPARE(l_replacement->written[1].fields()[1], QString("second"));
}

void tst_ClientSession::bufferIsBoundedAndRecordsOverflow()
{
    FakeTransport *l_transport = new FakeTransport(false);
    akashi::ClientSession l_session(nullptr, l_transport, 1);

    for (int i = 0; i < 600; i++) {
        l_session.write(akashi::Packet("CT", {"server", QString::number(i)}));
    }

    QCOMPARE(l_session.pending_packets.size(), 512);
    QVERIFY(l_session.pending_overflowed);
    // The oldest packets are the dropped ones.
    QCOMPARE(l_session.pending_packets.head().fields()[1], QString::number(600 - 512));
}

void tst_ClientSession::rebindReplacesAndDeletesOldTransport()
{
    FakeTransport *l_transport = new FakeTransport(true);
    akashi::ClientSession l_session(nullptr, l_transport, 1);
    QSignalSpy l_destroyed(l_transport, &QObject::destroyed);

    FakeTransport *l_replacement = new FakeTransport(true);
    l_session.bindTransport(l_replacement);

    QCOMPARE(l_session.transport, l_replacement);
    QCOMPARE(l_replacement->parent(), &l_session);
    // The old transport is deleted through the event loop.
    QTRY_COMPARE(l_destroyed.size(), 1);

    // Signals of a replaced transport no longer reach the session.
    l_session.write(akashi::Packet("CT", {"server", "current transport only"}));
    QCOMPARE(l_replacement->written.size(), 1);
}

void tst_ClientSession::forwardsTransportSignals()
{
    FakeTransport *l_transport = new FakeTransport(true);
    akashi::ClientSession l_session(nullptr, l_transport, 1);
    QSignalSpy l_packets(&l_session, &akashi::ClientSession::packetReceived);
    QSignalSpy l_closed(&l_session, &akashi::ClientSession::transportClosed);

    Q_EMIT l_transport->packetReceived(akashi::Packet("CH", {"1"}));
    l_transport->close();
    l_transport->close(); // a second close must not double-report

    QCOMPARE(l_packets.size(), 1);
    QCOMPARE(l_closed.size(), 1);
}

void tst_ClientSession::addsOnePlayerAndEnforcesTheCap()
{
    FakeTransport *l_transport = new FakeTransport(true);
    akashi::ClientSession l_session(nullptr, l_transport, 4);

    // The default character exists, reuses the session id, and is active.
    QCOMPARE(l_session.players.size(), 1);
    QCOMPARE(l_session.active_player, l_session.players.first());
    QCOMPARE(l_session.active_player->id(), 4);
    QCOMPARE(l_session.active_player->session(), &l_session);

    // The cap counts existing characters: full at 1, room at 2.
    QCOMPARE(l_session.addPlayer(9, 1), nullptr);
    akashi::PlayerState *l_second = l_session.addPlayer(9, 2);
    QVERIFY(l_second);
    QCOMPARE(l_second->id(), 9);
    QCOMPARE(l_session.players.size(), 2);
}

void tst_ClientSession::reportsTimeoutWhenNobodyReconnects()
{
    FakeTransport *l_transport = new FakeTransport(true);
    akashi::ClientSession l_session(nullptr, l_transport, 1);
    QSignalSpy l_timed_out(&l_session, &akashi::ClientSession::reconnectTimedOut);

    l_transport->loseConnection();
    l_session.waitForReconnect(0);

    QVERIFY(l_session.isWaitingForReconnect());
    QVERIFY(l_timed_out.wait(1000));
}

void tst_ClientSession::bindingANewTransportCancelsTheWait()
{
    FakeTransport *l_transport = new FakeTransport(true);
    akashi::ClientSession l_session(nullptr, l_transport, 1);
    QSignalSpy l_timed_out(&l_session, &akashi::ClientSession::reconnectTimedOut);

    l_transport->loseConnection();
    l_session.write(akashi::Packet("CT", {"server", "while you were away"}));
    l_session.waitForReconnect(60);

    FakeTransport *l_replacement = new FakeTransport(true);
    l_session.bindTransport(l_replacement);

    // The person is back: the wait ends, no timeout fires, and the packets
    // held while the connection was down arrive on the new one.
    QVERIFY(!l_session.isWaitingForReconnect());
    QVERIFY(!l_session.reconnect_timer->isActive());
    QCOMPARE(l_timed_out.size(), 0);
    QCOMPARE(l_replacement->written.size(), 1);
}

QTEST_MAIN(tst_ClientSession)
#include "tst_clientsession.moc"
