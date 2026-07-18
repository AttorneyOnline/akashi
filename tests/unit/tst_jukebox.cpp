// AI-generated: written by Claude.
#include "world/floor.h"
#include "world/jukebox.h"

#include <QSignalSpy>
#include <QTest>

using akashi::Floor;
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

    QList<std::pair<int, QString>> requests;
    QList<int> departures;
    QList<JukeboxSong> picks;
    int resets = 0;
};

static JukeboxSong song(const QString &f_name, int f_seconds = 100)
{
    return {f_name, f_name + ".opus", f_seconds};
}

static Floor makeFloor()
{
    Floor l_floor;
    l_floor.music_ordered = {"==Music==", "Announce The Truth (AJ).opus", "Pursuit.opus", "==Ambience==", "rain.opus"};
    l_floor.music_songs = {
        {"==Music==", {"==Music==", "==Music==", 0}},
        {"Announce The Truth (AJ).opus", {"Announce The Truth (AJ).opus", "Announce The Truth (AJ).opus", 180}},
        {"Pursuit.opus", {"Pursuit.opus", "Pursuit.opus", 120}},
        {"==Ambience==", {"==Ambience==", "==Ambience==", 0}},
        {"rain.opus", {"rain.opus", "rain.opus", 300}},
    };
    l_floor.approved_cdns = {"my.cdn.com", "your.cdn.com"};
    return l_floor;
}

class tst_Jukebox : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    // ── Playback engine ─────────────────────────────────────────
    void queueRefusesUnplayableAndDuplicateSongs();
    void firstRequestStartsPlaybackRightAway();
    void queueDrainsAndTheLastSongLoops();
    void skipAnswersWhetherAnythingPlays();
    void stopForgetsPendingSongsButKeepsThePolicy();
    void timerMovesToTheNextSongOnItsOwn();
    void policySeamCarriesEveryDecision();
    void playerDeparturesReachThePolicy();
    void refusedRequestStartsNothing();
    void playerLeftForAnUnknownPlayerChangesNothing();
    void nullPolicyKeepsTheCurrentOne();

    // ── Floor catalog ───────────────────────────────────────────
    void setFloorCatalogClearsCustomsAndEmits();
    void resolvedListReturnsFloorEntriesAlone();

    // ── Custom catalog ──────────────────────────────────────────
    void addSongAppendsCustomEntry();
    void addSongOverwritesSameName();
    void addSongRejectsBadCdn();
    void addSongAcceptsApprovedCdn();
    void addSongFailsClosedWithoutConfiguredCdns();
    void addSongRejectsEmptyName();
    void addCategoryWrapsInMarkers();
    void addCategoryRejectsExtension();
    void removeSongOnlyRemovesCustoms();
    void resetToFloorClearsCustoms();

    // ── Resolved catalog ────────────────────────────────────────
    void resolvedListMergesFloorAndCustom();
    void resolvedListCustomOverrideDoesNotDuplicate();
    void songInfoCustomOverridesFloor();
    void hasSongChecksBothLayers();

    // ── Category queries ────────────────────────────────────────
    void songsInCategoryFiltersCorrectly();
    void allPlayableSongsExcludesCategoriesAndZeroDuration();

    // ── Validation ──────────────────────────────────────────────
    void validateSongAcceptsLocalFiles();
    void validateSongRejectsUnknownExtension();
    void validateSongAcceptsApprovedCdnUrl();
    void validateSongRejectsUnapprovedCdnUrl();
    void validateSongRejectsBadScheme();

    // ── Music state ─────────────────────────────────────────────
    void changeMusicStoresState();
    void changeAmbienceEmitsSignal();

    // ── queueSong ───────────────────────────────────────────────
    void queueSongResolvesFromCatalog();
    void queueSongRejectsUnknownSong();
    void queueSongWithoutAnyCatalogRefuses();
};

// ── Playback engine ─────────────────────────────────────────────

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

    QCOMPARE(l_started.size(), 1);
    QVERIFY(l_jukebox.isPlaying());
    QCOMPARE(l_jukebox.currentSongName(), "first");

    // Later requests wait for their turn.
    l_jukebox.request(0, song("second"));
    QCOMPARE(l_started.size(), 1);
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
    QCOMPARE(l_started.size(), 1);

    // The one-second song ends and the jukebox rearms itself.
    QVERIFY(l_started.wait(2000));
    QCOMPARE(l_started.size(), 2);
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
    QCOMPARE(l_scripted->requests.first(), std::make_pair(7, QString("anything")));

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

void tst_Jukebox::refusedRequestStartsNothing()
{
    Jukebox l_jukebox;
    QSignalSpy l_started(&l_jukebox, &Jukebox::songStarted);

    // The refused song never reaches the queue, so the idle jukebox finds
    // nothing to play and stays silent.
    QCOMPARE(l_jukebox.request(0, song("broken", 0)), "Unable to add song. Duration shorter than 1.");
    QCOMPARE(l_started.size(), 0);
    QVERIFY(!l_jukebox.isPlaying());
    QCOMPARE(l_jukebox.pendingCount(), 0);
}

void tst_Jukebox::playerLeftForAnUnknownPlayerChangesNothing()
{
    Jukebox l_jukebox;
    l_jukebox.request(0, song("first"));

    // The queue policy ties nothing to players; an unknown departure is a
    // no-op for it and playback carries on.
    l_jukebox.playerLeft(99);
    QVERIFY(l_jukebox.isPlaying());
    QCOMPARE(l_jukebox.currentSongName(), "first");
    QCOMPARE(l_jukebox.pendingCount(), 1);
}

void tst_Jukebox::nullPolicyKeepsTheCurrentOne()
{
    Jukebox l_jukebox;
    l_jukebox.setPolicy(nullptr);

    // The default queue policy still answers.
    QCOMPARE(l_jukebox.request(0, song("first")), "Song added to Jukebox.");
}

// ── Floor catalog ───────────────────────────────────────────────

void tst_Jukebox::setFloorCatalogClearsCustomsAndEmits()
{
    Jukebox l_jukebox;
    Floor l_floor = makeFloor();

    l_jukebox.addSong({"custom.opus", "custom.opus", 60});
    QCOMPARE(l_jukebox.resolvedList().size(), 1);

    QSignalSpy l_spy(&l_jukebox, &Jukebox::musicListChanged);
    l_jukebox.setFloorCatalog(&l_floor);

    QCOMPARE(l_spy.size(), 1);
    QCOMPARE(l_jukebox.resolvedList().size(), 5);
    QVERIFY(!l_jukebox.hasSong("custom.opus"));
}

void tst_Jukebox::resolvedListReturnsFloorEntriesAlone()
{
    Jukebox l_jukebox;
    Floor l_floor = makeFloor();
    l_jukebox.setFloorCatalog(&l_floor);

    const QStringList l_list = l_jukebox.resolvedList();
    QCOMPARE(l_list, l_floor.music_ordered);
}

// ── Custom catalog ──────────────────────────────────────────────

void tst_Jukebox::addSongAppendsCustomEntry()
{
    Jukebox l_jukebox;
    Floor l_floor = makeFloor();
    l_jukebox.setFloorCatalog(&l_floor);

    QSignalSpy l_spy(&l_jukebox, &Jukebox::musicListChanged);
    const QString l_result = l_jukebox.addSong({"newsong.opus", "newsong.opus", 90});

    QCOMPARE(l_result, "Song added successfully.");
    QCOMPARE(l_spy.size(), 1);
    QVERIFY(l_jukebox.hasSong("newsong.opus"));
    QVERIFY(l_jukebox.resolvedList().contains("newsong.opus"));
}

void tst_Jukebox::addSongOverwritesSameName()
{
    Jukebox l_jukebox;
    Floor l_floor = makeFloor();
    l_jukebox.setFloorCatalog(&l_floor);

    l_jukebox.addSong({"Pursuit.opus", "Pursuit-remix.opus", 200});

    const auto l_info = l_jukebox.songInfo("Pursuit.opus");
    QVERIFY(l_info.has_value());
    QCOMPARE(l_info->real_name, "Pursuit-remix.opus");
    QCOMPARE(l_info->seconds, 200);

    // The resolved list must not duplicate the overridden entry.
    QCOMPARE(l_jukebox.resolvedList().count("Pursuit.opus"), 1);
}

void tst_Jukebox::addSongRejectsBadCdn()
{
    Jukebox l_jukebox;
    Floor l_floor = makeFloor();
    l_jukebox.setFloorCatalog(&l_floor);

    const QString l_result = l_jukebox.addSong({"https://evil.com/song.opus", "https://evil.com/song.opus", 60});
    QCOMPARE(l_result, "The song is not from an approved source.");
    QVERIFY(!l_jukebox.hasSong("https://evil.com/song.opus"));
}

void tst_Jukebox::addSongAcceptsApprovedCdn()
{
    Jukebox l_jukebox;
    Floor l_floor = makeFloor();
    l_jukebox.setFloorCatalog(&l_floor);

    const QString l_result = l_jukebox.addSong({"https://my.cdn.com/song.opus", "https://my.cdn.com/song.opus", 60});
    QCOMPARE(l_result, "Song added successfully.");
    QVERIFY(l_jukebox.hasSong("https://my.cdn.com/song.opus"));
}

void tst_Jukebox::addSongRejectsEmptyName()
{
    Jukebox l_jukebox;
    QCOMPARE(l_jukebox.addSong({"", "real.opus", 60}), "Song name cannot be empty.");
}

// With no approved CDN configured, addSong used to skip validation entirely -
// any URL from any host with any extension entered the persistent floor
// catalog, while /play rejected the very same source. Both fail closed now:
// no CDN list means no remote source is approved.
void tst_Jukebox::addSongFailsClosedWithoutConfiguredCdns()
{
    Jukebox l_jukebox;
    Floor l_floor = makeFloor();
    l_floor.approved_cdns.clear();
    l_jukebox.setFloorCatalog(&l_floor);

    QCOMPARE(l_jukebox.addSong({"https://evil.com/song.opus", "https://evil.com/song.opus", 60}),
             "The song is not from an approved source.");
    QCOMPARE(l_jukebox.addSong({"song.exe", "song.exe", 60}),
             "The song is not from an approved source.");
    // Local names with a playable extension still work without any CDN.
    QCOMPARE(l_jukebox.addSong({"local.opus", "local.opus", 60}), "Song added successfully.");

    // The same holds with no floor catalog at all.
    Jukebox l_bare;
    QCOMPARE(l_bare.addSong({"https://evil.com/song.opus", "https://evil.com/song.opus", 60}),
             "The song is not from an approved source.");
}

void tst_Jukebox::addCategoryWrapsInMarkers()
{
    Jukebox l_jukebox;
    QSignalSpy l_spy(&l_jukebox, &Jukebox::musicListChanged);

    const QString l_result = l_jukebox.addCategory("Custom Music");
    QCOMPARE(l_result, "Category added successfully.");
    QCOMPARE(l_spy.size(), 1);
    QVERIFY(l_jukebox.resolvedList().contains("==Custom Music=="));

    // Already-wrapped names stay as-is.
    l_jukebox.addCategory("==Already Wrapped==");
    QVERIFY(l_jukebox.resolvedList().contains("==Already Wrapped=="));
    QVERIFY(!l_jukebox.resolvedList().contains("====Already Wrapped===="));
}

void tst_Jukebox::addCategoryRejectsExtension()
{
    Jukebox l_jukebox;
    QCOMPARE(l_jukebox.addCategory("song.opus"), "Category names cannot contain a file extension.");
}

void tst_Jukebox::removeSongOnlyRemovesCustoms()
{
    Jukebox l_jukebox;
    Floor l_floor = makeFloor();
    l_jukebox.setFloorCatalog(&l_floor);

    // Cannot remove a floor entry.
    QVERIFY(!l_jukebox.removeSong("Pursuit.opus"));
    QVERIFY(l_jukebox.hasSong("Pursuit.opus"));

    // Can remove a custom entry.
    l_jukebox.addSong({"custom.opus", "custom.opus", 60});
    QVERIFY(l_jukebox.hasSong("custom.opus"));

    QSignalSpy l_spy(&l_jukebox, &Jukebox::musicListChanged);
    QVERIFY(l_jukebox.removeSong("custom.opus"));
    QCOMPARE(l_spy.size(), 1);
    QVERIFY(!l_jukebox.hasSong("custom.opus"));
}

void tst_Jukebox::resetToFloorClearsCustoms()
{
    Jukebox l_jukebox;
    Floor l_floor = makeFloor();
    l_jukebox.setFloorCatalog(&l_floor);

    l_jukebox.addSong({"a.opus", "a.opus", 10});
    l_jukebox.addSong({"b.opus", "b.opus", 20});
    l_jukebox.addCategory("Extra");
    QCOMPARE(l_jukebox.resolvedList().size(), 8);

    QSignalSpy l_spy(&l_jukebox, &Jukebox::musicListChanged);
    l_jukebox.resetToFloor();

    QCOMPARE(l_spy.size(), 1);
    QCOMPARE(l_jukebox.resolvedList(), l_floor.music_ordered);
    QVERIFY(!l_jukebox.hasSong("a.opus"));
}

// ── Resolved catalog ────────────────────────────────────────────

void tst_Jukebox::resolvedListMergesFloorAndCustom()
{
    Jukebox l_jukebox;
    Floor l_floor = makeFloor();
    l_jukebox.setFloorCatalog(&l_floor);

    l_jukebox.addSong({"custom.opus", "custom.opus", 60});

    const QStringList l_list = l_jukebox.resolvedList();
    // Floor entries come first, custom-only entries appended at the end.
    QCOMPARE(l_list.size(), 6);
    QCOMPARE(l_list.last(), "custom.opus");
    QCOMPARE(l_list.first(), "==Music==");
}

void tst_Jukebox::resolvedListCustomOverrideDoesNotDuplicate()
{
    Jukebox l_jukebox;
    Floor l_floor = makeFloor();
    l_jukebox.setFloorCatalog(&l_floor);

    // Overwrite an existing floor song with custom metadata.
    l_jukebox.addSong({"rain.opus", "rain-remix.opus", 400});

    const QStringList l_list = l_jukebox.resolvedList();
    // The entry should appear exactly once, at its original floor position.
    QCOMPARE(l_list.count("rain.opus"), 1);
    QCOMPARE(l_list.size(), 5);
}

void tst_Jukebox::songInfoCustomOverridesFloor()
{
    Jukebox l_jukebox;
    Floor l_floor = makeFloor();
    l_jukebox.setFloorCatalog(&l_floor);

    // Floor version.
    auto l_info = l_jukebox.songInfo("Pursuit.opus");
    QVERIFY(l_info.has_value());
    QCOMPARE(l_info->seconds, 120);

    // Custom override takes precedence.
    l_jukebox.addSong({"Pursuit.opus", "Pursuit-v2.opus", 999});
    l_info = l_jukebox.songInfo("Pursuit.opus");
    QVERIFY(l_info.has_value());
    QCOMPARE(l_info->real_name, "Pursuit-v2.opus");
    QCOMPARE(l_info->seconds, 999);

    // Unknown song returns nothing.
    QVERIFY(!l_jukebox.songInfo("nonexistent.opus").has_value());
}

void tst_Jukebox::hasSongChecksBothLayers()
{
    Jukebox l_jukebox;
    Floor l_floor = makeFloor();
    l_jukebox.setFloorCatalog(&l_floor);

    QVERIFY(l_jukebox.hasSong("Pursuit.opus"));
    QVERIFY(!l_jukebox.hasSong("custom.opus"));

    l_jukebox.addSong({"custom.opus", "custom.opus", 60});
    QVERIFY(l_jukebox.hasSong("custom.opus"));
}

// ── Category queries ────────────────────────────────────────────

void tst_Jukebox::songsInCategoryFiltersCorrectly()
{
    Jukebox l_jukebox;
    Floor l_floor = makeFloor();
    l_jukebox.setFloorCatalog(&l_floor);

    const QList<JukeboxSong> l_music = l_jukebox.songsInCategory("==Music==");
    QCOMPARE(l_music.size(), 2);
    QCOMPARE(l_music.at(0).name, "Announce The Truth (AJ).opus");
    QCOMPARE(l_music.at(1).name, "Pursuit.opus");

    const QList<JukeboxSong> l_ambience = l_jukebox.songsInCategory("==Ambience==");
    QCOMPARE(l_ambience.size(), 1);
    QCOMPARE(l_ambience.at(0).name, "rain.opus");

    // A nonexistent category returns nothing.
    QVERIFY(l_jukebox.songsInCategory("==Nope==").isEmpty());
}

void tst_Jukebox::allPlayableSongsExcludesCategoriesAndZeroDuration()
{
    Jukebox l_jukebox;
    Floor l_floor = makeFloor();
    l_jukebox.setFloorCatalog(&l_floor);

    const QList<JukeboxSong> l_all = l_jukebox.allPlayableSongs();
    // The floor has 3 real songs and 2 categories (duration 0).
    QCOMPARE(l_all.size(), 3);
    for (const JukeboxSong &l_song : l_all) {
        QVERIFY(l_song.seconds > 0);
        QVERIFY(!l_song.name.startsWith("=="));
    }
}

// ── Validation ──────────────────────────────────────────────────

void tst_Jukebox::validateSongAcceptsLocalFiles()
{
    QVERIFY(Jukebox::validateSong("Pursuit.opus", {}));
    QVERIFY(Jukebox::validateSong("song.ogg", {}));
    QVERIFY(Jukebox::validateSong("song.wav", {}));
    QVERIFY(Jukebox::validateSong("song.mp3", {}));
}

void tst_Jukebox::validateSongRejectsUnknownExtension()
{
    QVERIFY(!Jukebox::validateSong("song.exe", {}));
    QVERIFY(!Jukebox::validateSong("song.flac", {}));
    QVERIFY(!Jukebox::validateSong("song", {}));
}

void tst_Jukebox::validateSongAcceptsApprovedCdnUrl()
{
    const QStringList l_cdns = {"my.cdn.com"};
    QVERIFY(Jukebox::validateSong("https://my.cdn.com/song.opus", l_cdns));
    QVERIFY(Jukebox::validateSong("http://my.cdn.com/path/song.mp3", l_cdns));
}

void tst_Jukebox::validateSongRejectsUnapprovedCdnUrl()
{
    const QStringList l_cdns = {"my.cdn.com"};
    QVERIFY(!Jukebox::validateSong("https://evil.com/song.opus", l_cdns));
}

void tst_Jukebox::validateSongRejectsBadScheme()
{
    const QStringList l_cdns = {"my.cdn.com"};
    QVERIFY(!Jukebox::validateSong("ftp://my.cdn.com/song.opus", l_cdns));
}

// ── Music state ─────────────────────────────────────────────────

void tst_Jukebox::changeMusicStoresState()
{
    Jukebox l_jukebox;

    l_jukebox.changeMusic("Pursuit.opus", "Phoenix");
    QCOMPARE(l_jukebox.currentSong(), "Pursuit.opus");
    QCOMPARE(l_jukebox.musicPlayedBy(), "Phoenix");
}

void tst_Jukebox::changeAmbienceEmitsSignal()
{
    Jukebox l_jukebox;
    QSignalSpy l_spy(&l_jukebox, &Jukebox::ambienceChanged);

    l_jukebox.changeAmbience("rain.opus");
    QCOMPARE(l_jukebox.currentAmbience(), "rain.opus");
    QCOMPARE(l_spy.size(), 1);
    QCOMPARE(l_spy.at(0).at(0).toString(), "rain.opus");
}

// ── queueSong ───────────────────────────────────────────────────

void tst_Jukebox::queueSongResolvesFromCatalog()
{
    Jukebox l_jukebox;
    Floor l_floor = makeFloor();
    l_jukebox.setFloorCatalog(&l_floor);

    QSignalSpy l_started(&l_jukebox, &Jukebox::songStarted);
    const QString l_result = l_jukebox.queueSong(0, "Pursuit.opus");

    QCOMPARE(l_result, "Song added to Jukebox.");
    QCOMPARE(l_started.size(), 1);
    QCOMPARE(l_jukebox.currentSongName(), "Pursuit.opus");
}

void tst_Jukebox::queueSongRejectsUnknownSong()
{
    Jukebox l_jukebox;
    Floor l_floor = makeFloor();
    l_jukebox.setFloorCatalog(&l_floor);

    const QString l_result = l_jukebox.queueSong(0, "nonexistent.opus");
    QCOMPARE(l_result, "Song not found in the music list.");
}

void tst_Jukebox::queueSongWithoutAnyCatalogRefuses()
{
    Jukebox l_jukebox;
    QSignalSpy l_started(&l_jukebox, &Jukebox::songStarted);

    // No floor catalog, no customs: every name is unknown, nothing starts.
    QCOMPARE(l_jukebox.queueSong(0, "Pursuit.opus"), "Song not found in the music list.");
    QCOMPARE(l_started.size(), 0);
    QVERIFY(!l_jukebox.isPlaying());
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_Jukebox)

#include "tst_jukebox.moc"
