// AI-generated: written by Claude.
#include "testtools/fake_packet_context.h"
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

    void musicChangeCallsTheVerb();
    void musicChangeRespectsTheBlocks();
    void unknownArgumentMovesToTheNamedArea();
    void judgeSplashCallsTheVerb();
    void judgeSplashRespectsBlocksAndCooldown();
    void evidenceIsAddedWithAccess();
    void evidenceDescriptionPassesThrough();
    void penaltyCallsTheVerb();
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

void tst_AreaMusic::musicChangeCallsTheVerb()
{
    // The verb receives the raw list argument, the client's claimed
    // character id, and a defaulted effects field.
    FakeContext l_context;
    run(Packet("MC", {"song1.opus", "2"}), l_context);
    QCOMPARE(l_context.calls, QStringList({"playMusic:song1.opus|list|2|0"}));

    // The effects field passes through when the client sends one.
    FakeContext l_effects;
    run(Packet("MC", {"song1.opus", "2", "", "5"}), l_effects);
    QCOMPARE(l_effects.calls, QStringList({"playMusic:song1.opus|list|2|5"}));

    // A category pick reaches the verb unnormalized; stopping the music
    // is the verb's job now.
    FakeContext l_category;
    l_category.music_name_list << "== Music ==";
    run(Packet("MC", {"== Music ==", "0"}), l_category);
    QCOMPARE(l_category.calls, QStringList({"playMusic:== Music ==|list|0|0"}));
}

void tst_AreaMusic::musicChangeRespectsTheBlocks()
{
    // The session and person gates refuse before the verb runs.
    FakeContext l_spectator;
    l_spectator.spectator = true;
    run(Packet("MC", {"song1.opus", "0"}), l_spectator);
    QCOMPARE(l_spectator.calls, QStringList({"message:Spectators are blocked from changing the music."}));

    FakeContext l_locked;
    l_locked.area_act_allowed = false;
    run(Packet("MC", {"song1.opus", "0"}), l_locked);
    QCOMPARE(l_locked.calls, QStringList({"message:Spectators are blocked from changing the music."}));

    FakeContext l_dj;
    l_dj.dj_blocked = true;
    run(Packet("MC", {"song1.opus", "0"}), l_dj);
    QCOMPARE(l_dj.calls, QStringList({"message:You are blocked from changing the music."}));

    // The music rules live in the verb; its refusal comes back to the
    // sender as a server message.
    FakeContext l_disabled;
    l_disabled.music_refusal = "Music is disabled in this area.";
    run(Packet("MC", {"song1.opus", "0"}), l_disabled);
    QCOMPARE(l_disabled.calls, QStringList({"playMusic:song1.opus|list|0|0", "message:Music is disabled in this area."}));
}

void tst_AreaMusic::unknownArgumentMovesToTheNamedArea()
{
    FakeContext l_context;
    run(Packet("MC", {"Courtroom", "0"}), l_context);
    QCOMPARE(l_context.changed_area, 1);
    QVERIFY(l_context.area_broadcasts.isEmpty());

    // No area of that name either; nothing happens - no move, no verb,
    // no message back.
    FakeContext l_nowhere;
    run(Packet("MC", {"The Moon", "0"}), l_nowhere);
    QVERIFY(l_nowhere.calls.isEmpty());
    QCOMPARE(l_nowhere.changed_area, -100);
}

void tst_AreaMusic::judgeSplashCallsTheVerb()
{
    // The cooldown gates in the handler; the verb owns the rules, the
    // broadcast and the judge log.
    FakeContext l_context;
    run(Packet("RT", {"testimony1"}), l_context);
    QCOMPARE(l_context.calls, QStringList({"startWtceCooldown", "useWtce:testimony1"}));

    // A ruling keeps which verdict was picked.
    FakeContext l_ruling;
    run(Packet("RT", {"judgeruling", "1"}), l_ruling);
    QCOMPARE(l_ruling.calls, QStringList({"startWtceCooldown", "useWtce:judgeruling|1"}));
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

    // Still cooling down; dropped without a message, before the verb runs.
    FakeContext l_cooling;
    l_cooling.wtce_ready = false;
    run(Packet("RT", {"testimony1"}), l_cooling);
    QCOMPARE(l_cooling.calls, QStringList({"startWtceCooldown"}));

    // The wtce rules live in the verb; its refusal comes back to the
    // sender as a server message.
    FakeContext l_disabled;
    l_disabled.wtce_refusal = "WTCE animations have been disabled in this area.";
    run(Packet("RT", {"testimony1"}), l_disabled);
    QCOMPARE(l_disabled.calls, QStringList({"startWtceCooldown", "useWtce:testimony1", "message:WTCE animations have been disabled in this area."}));
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

void tst_AreaMusic::evidenceDescriptionPassesThrough()
{
    // The hidden-CM owner tagging lives in the session's evidence path
    // (EvidenceStore::taggedDescription, covered by tst_area); the handler
    // forwards the description verbatim.
    FakeContext l_context;
    run(Packet("PE", {"Knife", "<owner=def>\nA bloody knife.", "knife.png"}), l_context);
    QCOMPARE(l_context.added_evidence.at(1), QString("<owner=def>\nA bloody knife."));
}

void tst_AreaMusic::penaltyCallsTheVerb()
{
    FakeContext l_context;
    run(Packet("HP", {"1", "5"}), l_context);
    QCOMPARE(l_context.calls, QStringList({"changePenalty:1|5"}));

    // An unknown bar still reaches the verb; ignoring it while echoing
    // both bars is the verb's job now.
    FakeContext l_unknown;
    run(Packet("HP", {"3", "5"}), l_unknown);
    QCOMPARE(l_unknown.calls, QStringList({"changePenalty:3|5"}));
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

    // A rule refusal from the verb comes back as a server message.
    FakeContext l_refused;
    l_refused.penalty_refusal = "No penalties here.";
    run(Packet("HP", {"1", "5"}), l_refused);
    QCOMPARE(l_refused.calls, QStringList({"changePenalty:1|5", "message:No penalties here."}));
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_AreaMusic)

#include "tst_area_music.moc"
