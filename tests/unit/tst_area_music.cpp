// AI-generated: written by Claude.
#include "fake_packet_context.h"
#include "proto/area_music.h"
#include "proto/packet_codec.h"
#include "proto/packet_registry.h"

#include <QTest>

namespace tests {
namespace unittests {

using namespace akashi;

class tst_AreaMusic : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void initTestCase();

    void musicChangePlaysTheSong();
    void categorySelectionStopsTheMusic();
    void aliasedSongResolvesBeforeTheBroadcast();
    void musicChangeRespectsTheBlocks();
    void jukeboxInterceptsTheSong();
    void unknownArgumentMovesToTheNamedArea();
    void judgeSplashReachesTheRoom();
    void judgeSplashRespectsBlocksAndCooldown();
    void evidenceIsAddedWithAccess();
    void hiddenCmAreaTagsNewEvidence();
    void penaltyUpdatesAndEchoesBothBars();
    void penaltyRespectsJudgeBlocks();

  private:
    void run(const Packet &f_packet, FakeContext &f_context);

    PacketRegistry m_handlers;
    PacketCodecRegistry m_codecs;
};

void tst_AreaMusic::initTestCase()
{
    registerAreaMusicPackets(m_handlers, m_codecs);
}

void tst_AreaMusic::run(const Packet &f_packet, FakeContext &f_context)
{
    const auto l_spec = m_handlers.spec(f_packet.header());
    QVERIFY(l_spec.has_value());
    QVERIFY(f_packet.fieldCount() >= l_spec->min_args);

    const auto l_codec = m_codecs.resolve(f_context.profile()).codecFor(f_packet.header());
    QVERIFY(l_codec);
    const auto l_message = l_codec->decode(f_packet);
    if (!l_message) {
        return;
    }
    m_handlers.handler(f_packet.header())->handle(*l_message, f_context);
}

void tst_AreaMusic::musicChangePlaysTheSong()
{
    FakeContext l_context;
    l_context.current_character_name = "Nick";
    run(Packet("MC", {"song1.opus", "2"}), l_context);

    QCOMPARE(l_context.area_broadcasts.size(), 1);
    const QStringList l_expected = {"song1.opus", "2", "Nick", "1", "0", "0"};
    QCOMPARE(l_context.area_broadcasts.first().fields(), l_expected);
    QCOMPARE(l_context.recorded_music, QString("song1.opus"));
    // The effects field passes through when the client sends one.
    FakeContext l_effects;
    run(Packet("MC", {"song1.opus", "2", "", "5"}), l_effects);
    QCOMPARE(l_effects.area_broadcasts.first().field(5), QString("5"));
}

void tst_AreaMusic::categorySelectionStopsTheMusic()
{
    FakeContext l_context;
    l_context.music_name_list << "== Music ==";
    run(Packet("MC", {"== Music ==", "0"}), l_context);

    QCOMPARE(l_context.area_broadcasts.first().field(0), QString("~stop.mp3"));
    // The dummy track is not a real song, so no alias lookup happens.
    QVERIFY(!l_context.calls.contains("resolveSongAlias"));
}

void tst_AreaMusic::aliasedSongResolvesBeforeTheBroadcast()
{
    FakeContext l_context;
    l_context.music_name_list << "Objection.opus";
    l_context.song_aliases.insert("Objection.opus", "Ace Attorney/Objection.opus");
    run(Packet("MC", {"Objection.opus", "0"}), l_context);

    QCOMPARE(l_context.area_broadcasts.first().field(0), QString("Ace Attorney/Objection.opus"));
    QCOMPARE(l_context.recorded_music, QString("Ace Attorney/Objection.opus"));
}

void tst_AreaMusic::musicChangeRespectsTheBlocks()
{
    FakeContext l_spectator;
    l_spectator.spectator = true;
    run(Packet("MC", {"song1.opus", "0"}), l_spectator);
    QVERIFY(l_spectator.area_broadcasts.isEmpty());
    QCOMPARE(l_spectator.calls, QStringList({"message:Spectators are blocked from changing the music."}));

    FakeContext l_locked;
    l_locked.area_act_allowed = false;
    run(Packet("MC", {"song1.opus", "0"}), l_locked);
    QCOMPARE(l_locked.calls, QStringList({"message:Spectators are blocked from changing the music."}));

    FakeContext l_dj;
    l_dj.dj_blocked = true;
    run(Packet("MC", {"song1.opus", "0"}), l_dj);
    QCOMPARE(l_dj.calls, QStringList({"message:You are blocked from changing the music."}));

    // The music-allowed area setting is a floor rule now; a rule block
    // stops the change with its reason.
    FakeContext l_disabled;
    l_disabled.before_rule_block = "Music is disabled in this area.";
    run(Packet("MC", {"song1.opus", "0"}), l_disabled);
    QCOMPARE(l_disabled.calls, QStringList({"checkBeforeRule:music_changed", "message:Music is disabled in this area."}));
}

void tst_AreaMusic::jukeboxInterceptsTheSong()
{
    FakeContext l_context;
    l_context.jukebox_enabled = true;
    run(Packet("MC", {"song1.opus", "0"}), l_context);

    QCOMPARE(l_context.queued_jukebox_song, QString("song1.opus"));
    QVERIFY(l_context.calls.contains("message:Song added to the jukebox."));
    QVERIFY(l_context.area_broadcasts.isEmpty());
}

void tst_AreaMusic::unknownArgumentMovesToTheNamedArea()
{
    FakeContext l_context;
    run(Packet("MC", {"Courtroom", "0"}), l_context);
    QCOMPARE(l_context.changed_area, 1);
    QVERIFY(l_context.area_broadcasts.isEmpty());

    // No area of that name either; nothing happens.
    FakeContext l_nowhere;
    run(Packet("MC", {"The Moon", "0"}), l_nowhere);
    QVERIFY(l_nowhere.calls.isEmpty());
}

void tst_AreaMusic::judgeSplashReachesTheRoom()
{
    FakeContext l_context;
    run(Packet("RT", {"testimony1"}), l_context);
    QCOMPARE(l_context.area_broadcasts.first().fields(), QStringList({"testimony1"}));
    QCOMPARE(l_context.judge_actions, QStringList({"WT/CE"}));

    // A ruling keeps which verdict was picked.
    FakeContext l_ruling;
    run(Packet("RT", {"judgeruling", "1"}), l_ruling);
    QCOMPARE(l_ruling.area_broadcasts.first().fields(), QStringList({"judgeruling", "1"}));
}

void tst_AreaMusic::judgeSplashRespectsBlocksAndCooldown()
{
    FakeContext l_spectator;
    l_spectator.spectator = true;
    run(Packet("RT", {"testimony1"}), l_spectator);
    QCOMPARE(l_spectator.calls, QStringList({"message:Spectators are blocked from using the judge controls."}));

    FakeContext l_blocked;
    l_blocked.wtce_blocked = true;
    run(Packet("RT", {"testimony1"}), l_blocked);
    QCOMPARE(l_blocked.calls, QStringList({"message:You are blocked from using the judge controls."}));

    FakeContext l_disabled;
    l_disabled.wtce_allowed = false;
    run(Packet("RT", {"testimony1"}), l_disabled);
    QCOMPARE(l_disabled.calls, QStringList({"message:WTCE animations have been disabled in this area."}));

    // Still cooling down; dropped without a message.
    FakeContext l_cooling;
    l_cooling.wtce_ready = false;
    run(Packet("RT", {"testimony1"}), l_cooling);
    QVERIFY(l_cooling.calls.isEmpty());
}

void tst_AreaMusic::evidenceIsAddedWithAccess()
{
    FakeContext l_context;
    run(Packet("PE", {"Knife", "A bloody knife.", "knife.png"}), l_context);
    QCOMPARE(l_context.added_evidence, QStringList({"Knife", "A bloody knife.", "knife.png"}));

    // Evidence access is a floor rule now.
    FakeContext l_denied;
    l_denied.before_rule_block = "You are not allowed to modify the evidence here.";
    run(Packet("PE", {"Knife", "A bloody knife.", "knife.png"}), l_denied);
    QVERIFY(l_denied.added_evidence.isEmpty());
    QCOMPARE(l_denied.calls, QStringList({"checkBeforeRule:evidence_added", "message:You are not allowed to modify the evidence here."}));
}

void tst_AreaMusic::hiddenCmAreaTagsNewEvidence()
{
    FakeContext l_context;
    l_context.evidence_hidden_cm = true;
    run(Packet("PE", {"Knife", "A bloody knife.", "knife.png"}), l_context);
    QCOMPARE(l_context.added_evidence.at(1), QString("<owner=all>\nA bloody knife."));

    // An explicit owner tag is kept as sent.
    FakeContext l_tagged;
    l_tagged.evidence_hidden_cm = true;
    run(Packet("PE", {"Knife", "<owner=def>\nA bloody knife.", "knife.png"}), l_tagged);
    QCOMPARE(l_tagged.added_evidence.at(1), QString("<owner=def>\nA bloody knife."));
}

void tst_AreaMusic::penaltyUpdatesAndEchoesBothBars()
{
    FakeContext l_context;
    run(Packet("HP", {"1", "5"}), l_context);

    QCOMPARE(l_context.penalties.value(1), 5);
    QCOMPARE(l_context.area_broadcasts.size(), 2);
    QCOMPARE(l_context.area_broadcasts.at(0).fields(), QStringList({"1", "5"}));
    QCOMPARE(l_context.area_broadcasts.at(1).fields(), QStringList({"2", "10"}));
    QCOMPARE(l_context.judge_actions, QStringList({"updated the penalties"}));

    // An unknown bar changes nothing but still echoes both bars.
    FakeContext l_unknown;
    run(Packet("HP", {"3", "5"}), l_unknown);
    QVERIFY(!l_unknown.calls.contains("setPenalty"));
    QCOMPARE(l_unknown.area_broadcasts.size(), 2);
}

void tst_AreaMusic::penaltyRespectsJudgeBlocks()
{
    FakeContext l_spectator;
    l_spectator.spectator = true;
    run(Packet("HP", {"1", "5"}), l_spectator);
    QCOMPARE(l_spectator.calls, QStringList({"message:Spectators are blocked from using the judge controls."}));

    FakeContext l_blocked;
    l_blocked.wtce_blocked = true;
    run(Packet("HP", {"1", "5"}), l_blocked);
    QCOMPARE(l_blocked.calls, QStringList({"message:You are blocked from using the judge controls."}));
    QCOMPARE(l_blocked.penalties.value(1), 10);
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_AreaMusic)

#include "tst_area_music.moc"
