// AI-generated: written by Claude.
#include "core/client_session.h"
#include "core/player_state.h"
#include "core/player_state_observer.h"
#include "testtools/fake_transport.h"

#include <QTest>

using akashi::FakeTransport;

namespace tests {
namespace unittests {

class tst_PlayerStateObserver : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void unregisterUnknownPlayer();
    void registerSendsRosterAndAnnouncement();
    void changesReachEveryone();
    void unregisterAnnouncesRemovalOnce();
    void multiCharacterSessionHearsBroadcastsOnce();
    void clearDropsEveryPlayerSilentlyAndLeavesNoDanglingRefs();
};

// One person with one character, watching through a fake transport.
struct Person
{
    FakeTransport *transport;
    akashi::ClientSession *session;

    explicit Person(int id)
    {
        transport = new FakeTransport(true);
        session = new akashi::ClientSession(nullptr, transport, id);
    }
    ~Person() { delete session; }
    akashi::PlayerState *player() const { return session->active_player; }
    QStringList received() const
    {
        QStringList l_lines;
        for (const akashi::Packet &l_packet : transport->written) {
            l_lines.append(l_packet.serialize());
        }
        return l_lines;
    }
};

void tst_PlayerStateObserver::unregisterUnknownPlayer()
{
    PlayerStateObserver observer;
    Person alice(0);

    // A player that never registered must be ignored, repeatedly.
    observer.unregisterPlayer(alice.player());
    observer.unregisterPlayer(alice.player());

    QVERIFY(alice.transport->written.isEmpty());
}

void tst_PlayerStateObserver::registerSendsRosterAndAnnouncement()
{
    PlayerStateObserver observer;
    Person alice(0);
    Person bob(1);
    alice.player()->setOocName("alice");

    observer.registerPlayer(alice.player());

    // The first arrival gets the roster: their own entry and its four fields.
    QCOMPARE(alice.received(), QStringList({"PR#0#0#%",
                                            "PU#0#0#alice#%",
                                            "PU#0#1##%",
                                            "PU#0#2##%",
                                            "PU#0#3#0#%"}));

    alice.transport->written.clear();
    observer.registerPlayer(bob.player());

    // Everyone already watching hears the newcomer...
    QCOMPARE(alice.received(), QStringList({"PR#1#0#%"}));
    // ...and the newcomer gets the whole roster, themselves included.
    QCOMPARE(bob.received(), QStringList({"PR#0#0#%",
                                          "PU#0#0#alice#%",
                                          "PU#0#1##%",
                                          "PU#0#2##%",
                                          "PU#0#3#0#%",
                                          "PR#1#0#%",
                                          "PU#1#0##%",
                                          "PU#1#1##%",
                                          "PU#1#2##%",
                                          "PU#1#3#0#%"}));
}

void tst_PlayerStateObserver::changesReachEveryone()
{
    PlayerStateObserver observer;
    Person alice(0);
    Person bob(1);
    observer.registerPlayer(alice.player());
    observer.registerPlayer(bob.player());
    alice.transport->written.clear();
    bob.transport->written.clear();

    bob.player()->setCharacter("Phoenix");
    bob.player()->setCharacter("Phoenix"); // no change, no packet
    bob.player()->setAreaId(2);

    const QStringList l_expected = {"PU#1#1#Phoenix#%", "PU#1#3#2#%"};
    QCOMPARE(alice.received(), l_expected);
    QCOMPARE(bob.received(), l_expected);
}

void tst_PlayerStateObserver::unregisterAnnouncesRemovalOnce()
{
    PlayerStateObserver observer;
    Person alice(0);
    Person bob(1);
    observer.registerPlayer(alice.player());
    observer.registerPlayer(bob.player());
    alice.transport->written.clear();
    bob.transport->written.clear();

    observer.unregisterPlayer(alice.player());
    observer.unregisterPlayer(alice.player()); // a second unregister is ignored

    QCOMPARE(bob.received(), QStringList({"PR#0#1#%"}));
    // The leaver hears nothing, and their later changes stay silent.
    alice.player()->setOocName("still here?");
    QVERIFY(alice.transport->written.isEmpty());
    QCOMPARE(bob.received(), QStringList({"PR#0#1#%"}));
}

void tst_PlayerStateObserver::multiCharacterSessionHearsBroadcastsOnce()
{
    PlayerStateObserver observer;
    Person alice(0);
    akashi::PlayerState *l_second = alice.session->addPlayer(7, 2);
    QVERIFY(l_second);

    observer.registerPlayer(alice.player());
    observer.registerPlayer(l_second);
    Person bob(1);
    alice.transport->written.clear();

    observer.registerPlayer(bob.player());

    // Two characters, one person: the announcement arrives exactly once.
    QCOMPARE(alice.received(), QStringList({"PR#1#0#%"}));
}

// clear() is the shutdown path: it must drop every tracked player without a
// PR-remove broadcast (the sessions are being deleted, not leaving), and it
// must actually let go of the pointers. If it did not, deleting the sessions
// would leave the observer holding dangling PlayerState pointers that the next
// registerPlayer roster loop would walk into.
void tst_PlayerStateObserver::clearDropsEveryPlayerSilentlyAndLeavesNoDanglingRefs()
{
    PlayerStateObserver observer;
    auto *alice = new Person(0);
    auto *bob = new Person(1);
    observer.registerPlayer(alice->player());
    observer.registerPlayer(bob->player());
    alice->transport->written.clear();
    bob->transport->written.clear();

    observer.clear();
    // No teardown broadcast reaches anyone.
    QVERIFY(alice->transport->written.isEmpty());
    QVERIFY(bob->transport->written.isEmpty());

    // Free the sessions (and their PlayerState children). With the pointers
    // still tracked this would strand freed memory in the observer.
    delete alice;
    delete bob;

    auto *carol = new Person(2);
    observer.registerPlayer(carol->player());
    // The cleared roster held no one, so Carol sees only her own entry.
    QCOMPARE(carol->received(), QStringList({"PR#2#0#%",
                                             "PU#2#0##%",
                                             "PU#2#1##%",
                                             "PU#2#2##%",
                                             "PU#2#3#0#%"}));
    delete carol;
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_PlayerStateObserver)

#include "tst_playerstateobserver.moc"
