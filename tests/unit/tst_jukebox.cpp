// AI-generated: written by Claude.
#include "world/jukebox.h"

#include <QSignalSpy>
#include <QTest>

using akashi::Jukebox;
using akashi::JukeboxPolicy;
using akashi::JukeboxSong;

namespace tests {
namespace unittests {

// A policy the tests script, proving the jukebox consults its seam for
// every decision instead of deciding anything itself.
class ScriptedPolicy : public JukeboxPolicy
{
  public:
    QString onRequest(int f_player_id, const JukeboxSong &f_song) override
    {
        requests.append({f_player_id, f_song.name});
        return "scripted answer";
    }

    void onPlayerLeft(int f_player_id) override
    {
        departures.append(f_player_id);
    }

    std::optional<JukeboxSong> pickNext() override
    {
        if (picks.isEmpty()) {
            return std::nullopt;
        }
        return picks.takeFirst();
    }

    void reset() override
    {
        ++resets;
    }

    int pendingCount() const override
    {
        return picks.size();
    }

    QList<QPair<int, QString>> requests;
    QList<int> departures;
    QList<JukeboxSong> picks;
    int resets = 0;
};

static JukeboxSong song(const QString &f_name, int f_seconds = 100)
{
    return {f_name, f_name + ".opus", f_seconds};
}

class tst_Jukebox : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void queueRefusesUnplayableAndDuplicateSongs();
    void firstRequestStartsPlaybackRightAway();
    void queueDrainsAndTheLastSongLoops();
    void skipAnswersWhetherAnythingPlays();
    void stopForgetsPendingSongsButKeepsThePolicy();
    void timerMovesToTheNextSongOnItsOwn();
    void policySeamCarriesEveryDecision();
    void playerDeparturesReachThePolicy();
};

void tst_Jukebox::queueRefusesUnplayableAndDuplicateSongs()
{
    Jukebox l_jukebox;

    QCOMPARE(l_jukebox.request(0, song("nolength", 0)), "Unable to add song. Duration shorter than 1.");
    QCOMPARE(l_jukebox.request(0, song("unknown", -1)), "Unable to add song. Duration shorter than 1.");
    QCOMPARE(l_jukebox.request(0, song("first")), "Song added to Jukebox.");
    QCOMPARE(l_jukebox.request(1, song("first")), "Unable to add song. Song already in Jukebox.");
    QCOMPARE(l_jukebox.pendingCount(), 1);
}

void tst_Jukebox::firstRequestStartsPlaybackRightAway()
{
    Jukebox l_jukebox;
    QSignalSpy l_started(&l_jukebox, &Jukebox::songStarted);

    QVERIFY(!l_jukebox.isPlaying());
    l_jukebox.request(0, song("first"));

    QCOMPARE(l_started.count(), 1);
    QVERIFY(l_jukebox.isPlaying());
    QCOMPARE(l_jukebox.currentSongName(), "first");

    // Later requests wait for their turn.
    l_jukebox.request(0, song("second"));
    QCOMPARE(l_started.count(), 1);
}

void tst_Jukebox::queueDrainsAndTheLastSongLoops()
{
    Jukebox l_jukebox;
    QSignalSpy l_started(&l_jukebox, &Jukebox::songStarted);

    l_jukebox.request(0, song("first"));
    l_jukebox.request(0, song("second"));
    l_jukebox.request(0, song("third"));
    QCOMPARE(l_jukebox.pendingCount(), 3);

    // Every skip plays one of the queued songs; the queue drains to one
    // survivor that loops forever, so the room never falls silent.
    QSet<QString> l_played;
    for (int i = 0; i < 6; ++i) {
        QVERIFY(l_jukebox.skip());
        l_played.insert(l_jukebox.currentSongName());
    }
    QCOMPARE(l_jukebox.pendingCount(), 1);
    QCOMPARE(l_played.size(), 3);

    const QString l_survivor = l_jukebox.currentSongName();
    QVERIFY(l_jukebox.skip());
    QCOMPARE(l_jukebox.currentSongName(), l_survivor);
}

void tst_Jukebox::skipAnswersWhetherAnythingPlays()
{
    Jukebox l_jukebox;
    QVERIFY(!l_jukebox.skip());

    l_jukebox.request(0, song("first"));
    QVERIFY(l_jukebox.skip());
}

void tst_Jukebox::stopForgetsPendingSongsButKeepsThePolicy()
{
    Jukebox l_jukebox;
    l_jukebox.request(0, song("first"));
    l_jukebox.request(0, song("second"));

    l_jukebox.stop();
    QVERIFY(!l_jukebox.isPlaying());
    QCOMPARE(l_jukebox.currentSongName(), QString());
    QCOMPARE(l_jukebox.pendingCount(), 0);

    // Switched back on with nothing pending: silence, no crash.
    l_jukebox.start();
    QVERIFY(!l_jukebox.isPlaying());

    // The queue policy still answers requests afterwards.
    QCOMPARE(l_jukebox.request(0, song("third")), "Song added to Jukebox.");
    QCOMPARE(l_jukebox.currentSongName(), "third");
}

void tst_Jukebox::timerMovesToTheNextSongOnItsOwn()
{
    Jukebox l_jukebox;
    QSignalSpy l_started(&l_jukebox, &Jukebox::songStarted);

    l_jukebox.request(0, song("first", 1));
    QCOMPARE(l_started.count(), 1);

    // The one-second song ends and the jukebox rearms itself.
    QVERIFY(l_started.wait(2000));
    QCOMPARE(l_started.count(), 2);
    QVERIFY(l_jukebox.isPlaying());
}

void tst_Jukebox::policySeamCarriesEveryDecision()
{
    Jukebox l_jukebox;
    auto l_policy = std::make_unique<ScriptedPolicy>();
    ScriptedPolicy *l_scripted = l_policy.get();
    l_jukebox.setPolicy(std::move(l_policy));

    // The request answer comes from the policy, not the jukebox.
    l_scripted->picks.append(song("scripted"));
    QCOMPARE(l_jukebox.request(7, song("anything")), "scripted answer");
    QCOMPARE(l_scripted->requests.first(), qMakePair(7, QString("anything")));

    // The pick did too.
    QCOMPARE(l_jukebox.currentSongName(), "scripted");

    // A policy without a next pick means silence.
    QVERIFY(!l_jukebox.skip());
    QVERIFY(!l_jukebox.isPlaying());

    // Stopping resets the policy.
    l_jukebox.stop();
    QCOMPARE(l_scripted->resets, 1);
}

void tst_Jukebox::playerDeparturesReachThePolicy()
{
    Jukebox l_jukebox;
    auto l_policy = std::make_unique<ScriptedPolicy>();
    ScriptedPolicy *l_scripted = l_policy.get();
    l_jukebox.setPolicy(std::move(l_policy));

    l_jukebox.playerLeft(3);
    l_jukebox.playerLeft(5);
    QCOMPARE(l_scripted->departures, (QList<int>{3, 5}));
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_Jukebox)

#include "tst_jukebox.moc"
