// AI-generated: written by Claude.
#include "fake_transport.h"
#include "core/client_session.h"
#include "core/player_state.h"
#include "playerstateobserver.h"

#include <QTest>

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
};

// One person with one character, watching through a fake transport.
struct Person
{
    FakeTransport *transport;
    akashi::ClientSession *session;

    explicit Person(int id)
    {
        transport = new FakeTransport(true);
        session = new akashi::ClientSession(id, transport);
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

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_PlayerStateObserver)

#include "tst_playerstateobserver.moc"
