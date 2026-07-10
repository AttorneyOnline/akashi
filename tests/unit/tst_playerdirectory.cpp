// AI-generated: written by Claude.
#include "core/client_session.h"
#include "core/player_directory.h"
#include "testtools/fake_transport.h"

#include <QTest>

using akashi::FakeTransport;

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

  private:
    akashi::ClientSession *makeClient(int f_id);
};

akashi::ClientSession *tst_PlayerDirectory::makeClient(int f_id)
{
    return new akashi::ClientSession(nullptr, new FakeTransport(true), f_id);
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

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_PlayerDirectory)

#include "tst_playerdirectory.moc"
