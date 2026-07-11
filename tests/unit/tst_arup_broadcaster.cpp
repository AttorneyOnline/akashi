// AI-generated: written by Claude.
#include "core/arup_broadcaster.h"
#include "proto/packet.h"
#include "world/area.h"

#include <QSignalSpy>
#include <QTest>

namespace tests {
namespace unittests {

class tst_ArupBroadcaster : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void buildPlayerCount();
    void buildStatus();
    void buildCmFree();
    void buildCmWithOwners();
    void buildLock();
    void sendFullArupUnicasts();
    void broadcastNowSendsImmediately();
    void signalCoalescesWithinOneTurn();
    void multipleTypesDirtyFlushOnce();
    void ownerFormatterCalledPerOwner();
    void lockStringValues();
    void playerCountNeverNegative();
    void floorArupOnlyContainsFloorAreas();
    void flushBroadcastsPerFloor();
    void sendFullArupUsesFloor();
};

void tst_ArupBroadcaster::buildPlayerCount()
{
    akashi::Area a0(0, "Lobby", 0, 0);
    akashi::Area a1(1, "Court", 0, 1);
    a0.addPlayer(10);
    a0.addPlayer(20);
    a1.addPlayer(30);

    akashi::ArupBroadcaster b;
    b.addArea(&a0, 0);
    b.addArea(&a1, 0);

    const akashi::Packet p = b.buildArup(akashi::ArupBroadcaster::Type::PlayerCount);
    QCOMPARE(p.header(), QStringLiteral("ARUP"));
    QCOMPARE(p.fields(), QStringList({"0", "2", "1"}));
}

void tst_ArupBroadcaster::buildStatus()
{
    akashi::Area a0(0, "Lobby", 0, 0);
    akashi::Area a1(1, "Court", 0, 1);
    a1.setStatus("CASING");

    akashi::ArupBroadcaster b;
    b.addArea(&a0, 0);
    b.addArea(&a1, 0);

    const akashi::Packet p = b.buildArup(akashi::ArupBroadcaster::Type::Status);
    QCOMPARE(p.fields(), QStringList({"1", "IDLE", "CASING"}));
}

void tst_ArupBroadcaster::buildCmFree()
{
    akashi::Area a0(0, "Lobby", 0, 0);
    akashi::ArupBroadcaster b;
    b.addArea(&a0, 0);

    const akashi::Packet p = b.buildArup(akashi::ArupBroadcaster::Type::Cm);
    QCOMPARE(p.fields(), QStringList({"2", "FREE"}));
}

void tst_ArupBroadcaster::buildCmWithOwners()
{
    akashi::Area a0(0, "Lobby", 0, 0);
    a0.addOwner(5);
    a0.addOwner(12);

    akashi::ArupBroadcaster b;
    b.addArea(&a0, 0);
    b.setOwnerFormatter([](int id) -> QString {
        if (id == 5)
            return "[5] Phoenix";
        if (id == 12)
            return "[12] Miles";
        return {};
    });

    const akashi::Packet p = b.buildArup(akashi::ArupBroadcaster::Type::Cm);
    QCOMPARE(p.fields(), QStringList({"2", "[5] Phoenix, [12] Miles"}));
}

void tst_ArupBroadcaster::buildLock()
{
    akashi::Area a0(0, "Lobby", 0, 0);
    akashi::Area a1(1, "Court", 0, 1);
    a1.setLockState(akashi::Area::LockState::Locked);

    akashi::ArupBroadcaster b;
    b.addArea(&a0, 0);
    b.addArea(&a1, 0);

    const akashi::Packet p = b.buildArup(akashi::ArupBroadcaster::Type::Lock);
    QCOMPARE(p.fields(), QStringList({"3", "FREE", "LOCKED"}));
}

void tst_ArupBroadcaster::sendFullArupUnicasts()
{
    akashi::Area a0(0, "Lobby", 0, 0);
    akashi::ArupBroadcaster b;
    b.addArea(&a0, 0);

    QSignalSpy spy(&b, &akashi::ArupBroadcaster::arupUnicast);
    b.sendFullArup(42, 0);

    QCOMPARE(spy.count(), 4);
    QCOMPARE(spy.at(0).at(1).toInt(), 42);
    const auto p0 = spy.at(0).at(0).value<akashi::Packet>();
    const auto p1 = spy.at(1).at(0).value<akashi::Packet>();
    const auto p2 = spy.at(2).at(0).value<akashi::Packet>();
    const auto p3 = spy.at(3).at(0).value<akashi::Packet>();
    QCOMPARE(p0.fields().constFirst(), QStringLiteral("0"));
    QCOMPARE(p1.fields().constFirst(), QStringLiteral("1"));
    QCOMPARE(p2.fields().constFirst(), QStringLiteral("2"));
    QCOMPARE(p3.fields().constFirst(), QStringLiteral("3"));
}

void tst_ArupBroadcaster::broadcastNowSendsImmediately()
{
    akashi::Area a0(0, "Lobby", 0, 0);
    akashi::ArupBroadcaster b;
    b.addArea(&a0, 0);

    QSignalSpy spy(&b, &akashi::ArupBroadcaster::arupFloorBroadcast);
    b.broadcastNow(akashi::ArupBroadcaster::Type::Status);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(1).toInt(), 0);
    const auto p = spy.at(0).at(0).value<akashi::Packet>();
    QCOMPARE(p.fields(), QStringList({"1", "IDLE"}));
}

void tst_ArupBroadcaster::signalCoalescesWithinOneTurn()
{
    akashi::Area a0(0, "Lobby", 0, 0);
    akashi::ArupBroadcaster b;
    b.addArea(&a0, 0);

    QSignalSpy spy(&b, &akashi::ArupBroadcaster::arupFloorBroadcast);

    a0.addPlayer(1);
    a0.addPlayer(2);

    QCOMPARE(spy.count(), 0);
    QCoreApplication::processEvents();

    QCOMPARE(spy.count(), 1);
    const auto p = spy.at(0).at(0).value<akashi::Packet>();
    QCOMPARE(p.fields(), QStringList({"0", "2"}));
}

void tst_ArupBroadcaster::multipleTypesDirtyFlushOnce()
{
    akashi::Area a0(0, "Lobby", 0, 0);
    akashi::ArupBroadcaster b;
    b.addArea(&a0, 0);

    QSignalSpy spy(&b, &akashi::ArupBroadcaster::arupFloorBroadcast);

    a0.addPlayer(1);
    a0.setStatus("CASING");
    a0.setLockState(akashi::Area::LockState::Locked);

    QCOMPARE(spy.count(), 0);
    QCoreApplication::processEvents();

    QCOMPARE(spy.count(), 3);
    const auto p0 = spy.at(0).at(0).value<akashi::Packet>();
    const auto p1 = spy.at(1).at(0).value<akashi::Packet>();
    const auto p2 = spy.at(2).at(0).value<akashi::Packet>();
    QCOMPARE(p0.fields().constFirst(), QStringLiteral("0"));
    QCOMPARE(p1.fields().constFirst(), QStringLiteral("1"));
    QCOMPARE(p2.fields().constFirst(), QStringLiteral("3"));
}

void tst_ArupBroadcaster::ownerFormatterCalledPerOwner()
{
    akashi::Area a0(0, "Lobby", 0, 0);
    a0.addOwner(1);
    a0.addOwner(2);
    a0.addOwner(99);

    QSet<int> called_with;
    akashi::ArupBroadcaster b;
    b.addArea(&a0, 0);
    b.setOwnerFormatter([&called_with](int id) -> QString {
        called_with.insert(id);
        if (id == 99)
            return {};
        return "[" + QString::number(id) + "] char";
    });

    const akashi::Packet p = b.buildArup(akashi::ArupBroadcaster::Type::Cm);
    QCOMPARE(called_with, QSet<int>({1, 2, 99}));
    QCOMPARE(p.fields(), QStringList({"2", "[1] char, [2] char"}));
}

void tst_ArupBroadcaster::lockStringValues()
{
    akashi::Area a0(0, "A", 0, 0);
    akashi::Area a1(1, "B", 0, 1);
    akashi::Area a2(2, "C", 0, 2);
    a1.setLockState(akashi::Area::LockState::Spectatable);
    a2.setLockState(akashi::Area::LockState::Locked);

    akashi::ArupBroadcaster b;
    b.addArea(&a0, 0);
    b.addArea(&a1, 0);
    b.addArea(&a2, 0);

    const akashi::Packet p = b.buildArup(akashi::ArupBroadcaster::Type::Lock);
    QCOMPARE(p.fields(), QStringList({"3", "FREE", "SPECTATABLE", "LOCKED"}));
}

void tst_ArupBroadcaster::playerCountNeverNegative()
{
    akashi::Area a0(0, "Lobby", 0, 0);
    a0.removePlayer(42);

    akashi::ArupBroadcaster b;
    b.addArea(&a0, 0);

    const akashi::Packet p = b.buildArup(akashi::ArupBroadcaster::Type::PlayerCount);
    QCOMPARE(p.fields(), QStringList({"0", "0"}));
}

void tst_ArupBroadcaster::floorArupOnlyContainsFloorAreas()
{
    akashi::Area a0(0, "Lobby", 0, 0);
    akashi::Area a1(1, "Court", 1, 0);
    a0.addPlayer(10);
    a1.addPlayer(20);
    a1.addPlayer(30);

    akashi::ArupBroadcaster b;
    b.addArea(&a0, 0);
    b.addArea(&a1, 1);

    QCOMPARE(b.floorCount(), 2);

    const akashi::Packet p0 = b.buildFloorArup(akashi::ArupBroadcaster::Type::PlayerCount, 0);
    QCOMPARE(p0.fields(), QStringList({"0", "1"}));

    const akashi::Packet p1 = b.buildFloorArup(akashi::ArupBroadcaster::Type::PlayerCount, 1);
    QCOMPARE(p1.fields(), QStringList({"0", "2"}));
}

void tst_ArupBroadcaster::flushBroadcastsPerFloor()
{
    akashi::Area a0(0, "Lobby", 0, 0);
    akashi::Area a1(1, "Court", 1, 0);

    akashi::ArupBroadcaster b;
    b.addArea(&a0, 0);
    b.addArea(&a1, 1);

    QSignalSpy spy(&b, &akashi::ArupBroadcaster::arupFloorBroadcast);

    a0.addPlayer(1);
    QCoreApplication::processEvents();

    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(0).at(1).toInt(), 0);
    QCOMPARE(spy.at(1).at(1).toInt(), 1);
}

void tst_ArupBroadcaster::sendFullArupUsesFloor()
{
    akashi::Area a0(0, "Lobby", 0, 0);
    akashi::Area a1(1, "Court", 1, 0);
    a0.addPlayer(10);
    a1.addPlayer(20);

    akashi::ArupBroadcaster b;
    b.addArea(&a0, 0);
    b.addArea(&a1, 1);

    QSignalSpy spy(&b, &akashi::ArupBroadcaster::arupUnicast);
    b.sendFullArup(42, 1);

    QCOMPARE(spy.count(), 4);
    const auto p = spy.at(0).at(0).value<akashi::Packet>();
    QCOMPARE(p.fields(), QStringList({"0", "1"}));
}

} // namespace unittests
} // namespace tests

QTEST_MAIN(tests::unittests::tst_ArupBroadcaster)
#include "tst_arup_broadcaster.moc"
