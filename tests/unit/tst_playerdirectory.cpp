// AI-generated: written by Claude.
#include "akashi/service_registry.h"
#include "core/client_session.h"
#include "core/command_context.h"
#include "core/player_directory.h"
#include "core/player_state.h"
#include "testtools/fake_transport.h"

#include <QHostAddress>
#include <QTest>

using akashi::FakeTransport;
using akashi::PlayerDirectory;

namespace tests {
namespace unittests {

class tst_PlayerDirectory : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void handsOutLowestIdsFirst();
    void reusesAReturnedIdFirst();
    void lowestModePicksTheLowestFreeId();
    void refusesWhenFull();
    void looksUpClientsNullSafely();
    void removingAClientFreesItsId();
    void listsClientsOldestFirst();
    void resolvesAsAServiceFromTheRegistry();
    void ipKeyGroupsIpv4AndMappedIpv6();
    void countsConnectionsPerIp();

  private:
    akashi::ClientSession *makeClient(int f_id);
    akashi::ClientSession *makeClientWithIp(int f_id, const QString &f_ip);
};

akashi::ClientSession *tst_PlayerDirectory::makeClient(int f_id)
{
    return new akashi::ClientSession(nullptr, new FakeTransport(true), f_id);
}

akashi::ClientSession *tst_PlayerDirectory::makeClientWithIp(int f_id, const QString &f_ip)
{
    auto *l_transport = new FakeTransport(true);
    // The session reads the peer address once at construction.
    l_transport->setPeerAddress(QHostAddress(f_ip));
    return new akashi::ClientSession(nullptr, l_transport, f_id);
}

void tst_PlayerDirectory::handsOutLowestIdsFirst()
{
    PlayerDirectory l_directory;
    l_directory.setCapacity(3);

    QCOMPARE(l_directory.takeId(), 0);
    QCOMPARE(l_directory.takeId(), 1);
    QCOMPARE(l_directory.takeId(), 2);
}

void tst_PlayerDirectory::reusesAReturnedIdFirst()
{
    PlayerDirectory l_directory;
    l_directory.setCapacity(3);
    l_directory.takeId();
    l_directory.takeId();

    l_directory.returnId(0);

    QCOMPARE(l_directory.takeId(), 0);
    QCOMPARE(l_directory.takeId(), 2);
}

void tst_PlayerDirectory::lowestModePicksTheLowestFreeId()
{
    PlayerDirectory l_directory;
    l_directory.setCapacity(4);
    l_directory.takeId();
    l_directory.takeId();
    l_directory.takeId();

    // 1 and 0 come back in that order; last_freed would now hand out 0
    // first, lowest ignores recency and picks the lowest number.
    l_directory.returnId(1);
    l_directory.returnId(0);

    l_directory.setIdAssignment(PlayerDirectory::IdAssignment::Lowest);
    QCOMPARE(l_directory.takeId(), 0);
    QCOMPARE(l_directory.takeId(), 1);
    QCOMPARE(l_directory.takeId(), 3);
    QVERIFY(l_directory.isFull());

    // Switching back keeps working with the same pool.
    l_directory.returnId(2);
    l_directory.returnId(0);
    l_directory.setIdAssignment(PlayerDirectory::IdAssignment::LastFreed);
    QCOMPARE(l_directory.takeId(), 0);
}

void tst_PlayerDirectory::refusesWhenFull()
{
    PlayerDirectory l_directory;
    l_directory.setCapacity(1);

    QVERIFY(!l_directory.isFull());
    QCOMPARE(l_directory.takeId(), 0);
    QVERIFY(l_directory.isFull());
    QCOMPARE(l_directory.takeId(), -1);
}

void tst_PlayerDirectory::looksUpClientsNullSafely()
{
    PlayerDirectory l_directory;
    l_directory.setCapacity(3);
    const int l_id = l_directory.takeId();
    akashi::ClientSession *l_client = makeClient(l_id);
    l_directory.addClient(l_id, l_client);

    QCOMPARE(l_directory.clientById(l_id), l_client);
    // A taken-but-unbound ID, a free ID, and an ID that never existed all
    // give nullptr - never a stale or filler pointer.
    QCOMPARE(l_directory.clientById(l_directory.takeId()), nullptr);
    QCOMPARE(l_directory.clientById(2), nullptr);
    QCOMPARE(l_directory.clientById(999), nullptr);
    QCOMPARE(l_directory.clientById(-1), nullptr);

    delete l_client;
}

void tst_PlayerDirectory::removingAClientFreesItsId()
{
    PlayerDirectory l_directory;
    l_directory.setCapacity(1);
    const int l_id = l_directory.takeId();
    akashi::ClientSession *l_client = makeClient(l_id);
    l_directory.addClient(l_id, l_client);
    QVERIFY(l_directory.isFull());

    l_directory.removeClient(l_id);
    l_directory.removeClient(l_id); // removing twice is ignored

    QCOMPARE(l_directory.clientById(l_id), nullptr);
    QCOMPARE(l_directory.clientCount(), 0);
    QVERIFY(!l_directory.isFull());
    QCOMPARE(l_directory.takeId(), l_id);

    delete l_client;
}

void tst_PlayerDirectory::listsClientsOldestFirst()
{
    PlayerDirectory l_directory;
    l_directory.setCapacity(3);
    akashi::ClientSession *l_first = makeClient(l_directory.takeId());
    akashi::ClientSession *l_second = makeClient(l_directory.takeId());
    l_directory.addClient(0, l_first);
    l_directory.addClient(1, l_second);

    QCOMPARE(l_directory.clients(), QVector<akashi::ClientSession *>({l_first, l_second}));
    QCOMPARE(l_directory.clientCount(), 2);

    l_directory.clear();
    QCOMPARE(l_directory.clientCount(), 0);

    delete l_first;
    delete l_second;
}

void tst_PlayerDirectory::resolvesAsAServiceFromTheRegistry()
{
    // The directory is a member of the server, so it registers with the
    // non-owning deleter and plugins resolve it as akashi.players.
    PlayerDirectory l_directory;
    l_directory.setCapacity(2);
    akashi::ClientSession *l_client = makeClient(l_directory.takeId());
    l_directory.addClient(0, l_client);

    akashi::ServiceRegistry l_registry;
    QVERIFY(l_registry.registerService(std::shared_ptr<PlayerDirectory>(&l_directory, [](auto *) {})));

    auto l_resolved = l_registry.resolve<PlayerDirectory>(QStringLiteral("akashi.players"));
    QVERIFY(l_resolved);
    QCOMPARE(l_resolved.get(), &l_directory);
    QCOMPARE(l_resolved->clients(), QVector<akashi::ClientSession *>({l_client}));
    QVERIFY(l_resolved->serviceVersion().satisfies(QStringLiteral("^1.0.0")));

    // The plugin-side walk: wrap the opaque session in a TargetPlayer and
    // read names through its characters.
    akashi::TargetPlayer l_player(l_resolved->clients().constFirst());
    QCOMPARE(l_player.players().size(), 1);
    l_player.players().constFirst()->setOocName(QStringLiteral("Phoenix"));
    QCOMPARE(l_player.players().constFirst()->oocName(), QStringLiteral("Phoenix"));

    delete l_client;
}

void tst_PlayerDirectory::ipKeyGroupsIpv4AndMappedIpv6()
{
    // IPv4 and its IPv4-mapped IPv6 form are the same origin, so they must
    // key identically (the old scan used isEqual's tolerant comparison).
    QCOMPARE(PlayerDirectory::ipKey(QHostAddress(QStringLiteral("1.2.3.4"))),
             PlayerDirectory::ipKey(QHostAddress(QStringLiteral("::ffff:1.2.3.4"))));
    // Different addresses key apart; a real IPv6 keys apart from IPv4.
    QVERIFY(PlayerDirectory::ipKey(QHostAddress(QStringLiteral("1.2.3.4"))) !=
            PlayerDirectory::ipKey(QHostAddress(QStringLiteral("1.2.3.5"))));
    QVERIFY(PlayerDirectory::ipKey(QHostAddress(QStringLiteral("::1"))) !=
            PlayerDirectory::ipKey(QHostAddress(QStringLiteral("1.2.3.4"))));
}

void tst_PlayerDirectory::countsConnectionsPerIp()
{
    PlayerDirectory l_directory;
    l_directory.setCapacity(5);
    akashi::ClientSession *l_a1 = makeClientWithIp(l_directory.takeId(), QStringLiteral("1.2.3.4"));
    akashi::ClientSession *l_a2 = makeClientWithIp(l_directory.takeId(), QStringLiteral("1.2.3.4"));
    akashi::ClientSession *l_b1 = makeClientWithIp(l_directory.takeId(), QStringLiteral("5.6.7.8"));
    l_directory.addClient(0, l_a1);
    l_directory.addClient(1, l_a2);
    l_directory.addClient(2, l_b1);

    const QHostAddress l_a(QStringLiteral("1.2.3.4"));
    const QHostAddress l_b(QStringLiteral("5.6.7.8"));
    QCOMPARE(l_directory.sameIpCount(l_a), 2);
    QCOMPARE(l_directory.sameIpCount(l_b), 1);
    QCOMPARE(l_directory.sameIpCount(QHostAddress(QStringLiteral("9.9.9.9"))), 0);
    // The mapped-IPv6 form of an address sees the same tally.
    QCOMPARE(l_directory.sameIpCount(QHostAddress(QStringLiteral("::ffff:1.2.3.4"))), 2);

    // Removal decrements; the key is dropped when it hits zero.
    l_directory.removeClient(0);
    QCOMPARE(l_directory.sameIpCount(l_a), 1);
    l_directory.removeClient(1);
    QCOMPARE(l_directory.sameIpCount(l_a), 0);
    QCOMPARE(l_directory.sameIpCount(l_b), 1);

    // clear() forgets every tally.
    l_directory.clear();
    QCOMPARE(l_directory.sameIpCount(l_b), 0);

    delete l_a1;
    delete l_a2;
    delete l_b1;
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_PlayerDirectory)

#include "tst_playerdirectory.moc"
