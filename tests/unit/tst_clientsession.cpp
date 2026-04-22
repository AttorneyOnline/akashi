// AI-generated: written by Claude.
#include "fake_transport.h"
#include "core/client_session.h"
#include "core/player_state.h"
#include "proto/packet.h"

#include <QSignalSpy>
#include <QTest>

class tst_ClientSession : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void writesThroughOpenTransport();
    void buffersWhileWireIsDown();
    void rebindReplaysPendingInOrder();
    void bufferIsBoundedAndRecordsOverflow();
    void rebindReplacesAndDeletesOldTransport();
    void forwardsTransportSignals();
    void spawnsOnePlayerAndEnforcesTheCap();
};

void tst_ClientSession::writesThroughOpenTransport()
{
    FakeTransport *l_transport = new FakeTransport(true);
    akashi::ClientSession l_session(1, l_transport);

    l_session.write(akashi::Packet("CT", {"server", "hello"}));

    QCOMPARE(l_transport->written.size(), 1);
    QCOMPARE(l_session.pending_packets.size(), 0);
}

void tst_ClientSession::buffersWhileWireIsDown()
{
    FakeTransport *l_transport = new FakeTransport(true);
    akashi::ClientSession l_session(1, l_transport);
    l_transport->close();

    l_session.write(akashi::Packet("CT", {"server", "missed you"}));

    QCOMPARE(l_transport->written.size(), 0);
    QCOMPARE(l_session.pending_packets.size(), 1);
}

void tst_ClientSession::rebindReplaysPendingInOrder()
{
    FakeTransport *l_transport = new FakeTransport(true);
    akashi::ClientSession l_session(1, l_transport);
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
    akashi::ClientSession l_session(1, l_transport);

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
    akashi::ClientSession l_session(1, l_transport);
    QSignalSpy l_destroyed(l_transport, &QObject::destroyed);

    FakeTransport *l_replacement = new FakeTransport(true);
    l_session.bindTransport(l_replacement);

    QCOMPARE(l_session.transport, l_replacement);
    QCOMPARE(l_replacement->parent(), &l_session);
    // The old wire is deleted through the event loop.
    QTRY_COMPARE(l_destroyed.size(), 1);

    // Signals of a replaced transport no longer reach the session.
    l_session.write(akashi::Packet("CT", {"server", "current wire only"}));
    QCOMPARE(l_replacement->written.size(), 1);
}

void tst_ClientSession::forwardsTransportSignals()
{
    FakeTransport *l_transport = new FakeTransport(true);
    akashi::ClientSession l_session(1, l_transport);
    QSignalSpy l_packets(&l_session, &akashi::ClientSession::packetReceived);
    QSignalSpy l_closed(&l_session, &akashi::ClientSession::transportClosed);

    Q_EMIT l_transport->packetReceived(akashi::Packet("CH", {"1"}));
    l_transport->close();
    l_transport->close(); // a second close must not double-report

    QCOMPARE(l_packets.size(), 1);
    QCOMPARE(l_closed.size(), 1);
}

void tst_ClientSession::spawnsOnePlayerAndEnforcesTheCap()
{
    FakeTransport *l_transport = new FakeTransport(true);
    akashi::ClientSession l_session(4, l_transport);

    // The default character exists, reuses the session id, and is active.
    QCOMPARE(l_session.players.size(), 1);
    QCOMPARE(l_session.active_player, l_session.players.first());
    QCOMPARE(l_session.active_player->id(), 4);
    QCOMPARE(l_session.active_player->session(), &l_session);

    // The cap counts existing characters: full at 1, room at 2.
    QCOMPARE(l_session.spawnPlayer(9, 1), nullptr);
    akashi::PlayerState *l_second = l_session.spawnPlayer(9, 2);
    QVERIFY(l_second);
    QCOMPARE(l_second->id(), 9);
    QCOMPARE(l_session.players.size(), 2);
}

QTEST_MAIN(tst_ClientSession)
#include "tst_clientsession.moc"
