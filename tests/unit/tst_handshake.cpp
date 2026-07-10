// AI-generated: written by Claude.
#include "testtools/fake_packet_context.h"
#include "proto/handshake.h"
#include "proto/packet_codec.h"
#include "proto/packet_registry.h"

#include <QTest>

namespace tests {
namespace unittests {

using namespace akashi;

class tst_Handshake : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void initTestCase();

    void helloStoresHwidAndRepliesWithId();
    void helloRefusesEmptyAndDoubleHwid();
    void helloTurnsAwayBannedHardware();
    void identifyAcceptsAo2AndDescribesServer();
    void identifyRefusesDoubleAndUnparseableVersions();
    void identifyRefusesWebaoWhenDisabled();
    void resourceCountsMatchTheLists();
    void characterAndMusicListsAreSent();
    void joinRunsJoinRulesBeforeDone();
    void joinRefusedByServerRule();
    void joinNeedsAHwidAndHappensOnce();
    void characterSelectFollowsTheOldRules();
    void keepaliveAnswersWithCheck();
    void characterPasswordIsRemembered();

  private:
    // Runs a packet the way the dispatcher does: resolve, decode, handle.
    void run(const Packet &f_packet, FakeContext &f_context);

    PacketRegistry m_handlers;
    PacketCodecRegistry m_codecs;
};

void tst_Handshake::initTestCase()
{
    registerHandshakePackets(m_handlers, m_codecs);
}

void tst_Handshake::run(const Packet &f_packet, FakeContext &f_context)
{
    const auto l_spec = m_handlers.spec(f_packet.header());
    QVERIFY(l_spec.has_value());
    QVERIFY(f_packet.fieldCount() >= l_spec->min_args);

    const auto l_codec = m_codecs.resolve(f_context.profile()).codecFor(f_packet.header());
    QVERIFY(l_codec);
    const auto l_message = l_codec->decode(f_packet);
    QVERIFY(l_message);
    m_handlers.handler(f_packet.header())->handle(*l_message, f_context);
}

void tst_Handshake::helloStoresHwidAndRepliesWithId()
{
    FakeContext l_context;
    run(Packet("HI", {"hwid123"}), l_context);

    QCOMPARE(l_context.stored_hwid, QString("hwid123"));
    QVERIFY(!l_context.closed);
    // The ban check happens after the attempt is logged.
    QCOMPARE(l_context.calls, QStringList({"setHwid", "logConnectionAttempt", "send:ID"}));
    QCOMPARE(l_context.sent.at(0).field(0), QString("5"));
    QCOMPARE(l_context.sent.at(0).field(1), QString("akashi"));
}

void tst_Handshake::helloRefusesEmptyAndDoubleHwid()
{
    FakeContext l_empty;
    run(Packet("HI", {""}), l_empty);
    QVERIFY(l_empty.closed);
    QCOMPARE(l_empty.sent.at(0).header(), QString("BD"));

    FakeContext l_double;
    l_double.stored_hwid = "already-set";
    run(Packet("HI", {"hwid123"}), l_double);
    QVERIFY(l_double.closed);
    QCOMPARE(l_double.stored_hwid, QString("already-set"));
    // The double send is a protocol error, not a silent close.
    QCOMPARE(l_double.sent.at(0).header(), QString("BD"));
    QCOMPARE(l_double.sent.at(0).field(0), QString("A protocol error has been encountered. Packet : HI"));
}

void tst_Handshake::helloTurnsAwayBannedHardware()
{
    FakeContext l_context;
    l_context.ban = BanRecord{99, "Naughty", QDateTime(), true};
    run(Packet("HI", {"hwid123"}), l_context);

    QVERIFY(l_context.closed);
    const QString l_notice = l_context.sent.at(0).field(0);
    QVERIFY(l_notice.contains("Reason: Naughty"));
    QVERIFY(l_notice.contains("Ban ID: 99"));
    QVERIFY(l_notice.contains("Until: Permanently."));
}

void tst_Handshake::identifyAcceptsAo2AndDescribesServer()
{
    FakeContext l_context;
    run(Packet("ID", {"AO2", "2.10.1"}), l_context);

    QVERIFY(!l_context.closed);
    QVERIFY(l_context.identified);
    QCOMPARE(l_context.stored_profile.arch, QString("AO2"));
    QVERIFY(l_context.stored_profile.version.atLeast(2, 10, 1));

    // PN with the live player count, the feature list, and the asset link.
    QCOMPARE(l_context.sent.size(), 3);
    QCOMPARE(l_context.sent.at(0).header(), QString("PN"));
    QCOMPARE(l_context.sent.at(0).field(0), QString("3"));
    QCOMPARE(l_context.sent.at(0).field(1), QString("100"));
    QCOMPARE(l_context.sent.at(1).header(), QString("FL"));
    QCOMPARE(l_context.sent.at(1).fieldCount(), 20);
    // The one auth token, built from the context's active system.
    QCOMPARE(l_context.sent.at(1).field(19), QString("auth_password"));
    QCOMPARE(l_context.sent.at(2).header(), QString("ASS"));
    QCOMPARE(l_context.sent.at(2).field(0), QString("http://attorneyoffline.de/base/"));
}

void tst_Handshake::identifyRefusesDoubleAndUnparseableVersions()
{
    FakeContext l_double;
    l_double.identified = true;
    run(Packet("ID", {"AO2", "2.10.1"}), l_double);
    QVERIFY(l_double.closed);
    QCOMPARE(l_double.sent.at(0).header(), QString("BD"));

    // Any release that parses is accepted - a release-1 client (e.g. DRO) too.
    FakeContext l_dro;
    run(Packet("ID", {"AO2", "1.7.5"}), l_dro);
    QVERIFY(!l_dro.closed);
    QVERIFY(l_dro.identified);

    // Only a version field with no parseable X.Y.Z is refused.
    FakeContext l_garbage;
    run(Packet("ID", {"AO2", "banana"}), l_garbage);
    QVERIFY(l_garbage.closed);
    QVERIFY(!l_garbage.identified);
    QCOMPARE(l_garbage.sent.at(0).header(), QString("BD"));
    QCOMPARE(l_garbage.sent.at(0).field(0), QString("A protocol error has been encountered. Packet : ID\nVersion not recognised."));
}

void tst_Handshake::identifyRefusesWebaoWhenDisabled()
{
    FakeContext l_webao;
    l_webao.webao_enabled = false;
    run(Packet("ID", {"webAO", "2.10.1"}), l_webao);
    QVERIFY(l_webao.closed);
    QVERIFY(!l_webao.identified);
    QCOMPARE(l_webao.sent.at(0).header(), QString("BD"));
    QCOMPARE(l_webao.sent.at(0).field(0), QString("WebAO is disabled on this server."));

    // The switch only turns away the webAO arch; desktop clients pass.
    FakeContext l_desktop;
    l_desktop.webao_enabled = false;
    run(Packet("ID", {"AO2", "2.10.1"}), l_desktop);
    QVERIFY(!l_desktop.closed);
    QVERIFY(l_desktop.identified);
}

void tst_Handshake::resourceCountsMatchTheLists()
{
    FakeContext l_context;
    run(Packet("askchaa"), l_context);

    QCOMPARE(l_context.sent.at(0).header(), QString("SI"));
    QCOMPARE(l_context.sent.at(0).field(0), QString("3"));
    QCOMPARE(l_context.sent.at(0).field(1), QString("0"));
    // Two areas plus two songs.
    QCOMPARE(l_context.sent.at(0).field(2), QString("4"));
}

void tst_Handshake::characterAndMusicListsAreSent()
{
    FakeContext l_context;
    run(Packet("RC"), l_context);
    QCOMPARE(l_context.sent.at(0).header(), QString("SC"));
    QCOMPARE(l_context.sent.at(0).fields(), l_context.character_list);

    FakeContext l_music;
    run(Packet("RM"), l_music);
    QCOMPARE(l_music.sent.at(0).header(), QString("SM"));
    QCOMPARE(l_music.sent.at(0).fields(), l_music.area_name_list + l_music.music_name_list);
}

void tst_Handshake::joinRunsJoinRulesBeforeDone()
{
    FakeContext l_context;
    l_context.stored_hwid = "hwid123";

    run(Packet("RD"), l_context);

    QVERIFY(l_context.joined);
    QVERIFY(!l_context.closed);
    // The area's state travels through the player_joined after-rules, which
    // must run while the client still shows its loading screen.
    const QStringList l_expected = {
        "checkBeforeRule:server_joined",
        "markJoined", "announceCharsTaken",
        "runAfterRule:player_joined",
        "send:TI", "send:DONE",
        "message:=== MOTD ===\r\nMOTD is not set.\r\n=============",
        "runAfterRule:server_joined",
        "finishJoin", "broadcastPlayerCount"};
    QCOMPARE(l_context.calls, l_expected);

    // The stopped global timer is the only packet the handler still builds.
    QCOMPARE(l_context.sent.at(0).fields(), QStringList({"0", "3"}));
}

void tst_Handshake::joinRefusedByServerRule()
{
    FakeContext l_context;
    l_context.stored_hwid = "hwid123";
    l_context.before_rule_block = "The whole floor is locked down.";

    run(Packet("RD"), l_context);

    // Turned away like a ban: reason shown, never counted as joined.
    QVERIFY(!l_context.joined);
    QVERIFY(l_context.closed);
    QCOMPARE(l_context.calls, QStringList({"checkBeforeRule:server_joined", "send:BD", "close"}));
    QCOMPARE(l_context.sent.at(0).fields(), QStringList({"The whole floor is locked down."}));
}

void tst_Handshake::joinNeedsAHwidAndHappensOnce()
{
    FakeContext l_early;
    run(Packet("RD"), l_early);
    QVERIFY(l_early.closed);
    QVERIFY(!l_early.joined);
    QVERIFY(l_early.sent.isEmpty());

    FakeContext l_again;
    l_again.stored_hwid = "hwid123";
    l_again.joined = true;
    run(Packet("RD"), l_again);
    QCOMPARE(l_again.calls, QStringList());
}

// Pre-join CC is refused by the spec's user permission at dispatch, so the
// handler itself no longer reads the joined flag.
void tst_Handshake::characterSelectFollowsTheOldRules()
{
    FakeContext l_context;
    run(Packet("CC", {"0", "1", "pass"}), l_context);
    QCOMPARE(l_context.selected_char_id, 1);
    QVERIFY(!l_context.closed);

    // A non-numeric choice means spectating.
    FakeContext l_spectator;
    run(Packet("CC", {"0", "what", "pass"}), l_spectator);
    QCOMPARE(l_spectator.selected_char_id, -1);

    // Out of range closes, but the selection still runs, like it always has.
    FakeContext l_out_of_range;
    run(Packet("CC", {"0", "12", "pass"}), l_out_of_range);
    QVERIFY(l_out_of_range.closed);
    QCOMPARE(l_out_of_range.selected_char_id, 12);
    QCOMPARE(l_out_of_range.sent.at(0).header(), QString("KK"));

    // The hostile below-minus-one path gets the same KK and close; the
    // selection seam still sees the raw id (the verb refuses it inside).
    FakeContext l_negative;
    run(Packet("CC", {"0", "-5", "pass"}), l_negative);
    QVERIFY(l_negative.closed);
    QCOMPARE(l_negative.sent.at(0).header(), QString("KK"));
    QCOMPARE(l_negative.selected_char_id, -5);

    // An in-range pick the verb refuses (a taken character) ends quietly:
    // no KK, no close, no message - the client just keeps its select screen.
    FakeContext l_taken;
    l_taken.select_character_result = false;
    run(Packet("CC", {"0", "1", "pass"}), l_taken);
    QVERIFY(!l_taken.closed);
    QCOMPARE(l_taken.calls, QStringList({"selectCharacter"}));
}

void tst_Handshake::keepaliveAnswersWithCheck()
{
    FakeContext l_context;
    run(Packet("CH", {"0"}), l_context);
    QCOMPARE(l_context.calls, QStringList({"send:CHECK"}));
}

void tst_Handshake::characterPasswordIsRemembered()
{
    FakeContext l_context;
    run(Packet("PW", {"hunter2"}), l_context);
    QCOMPARE(l_context.character_password, QString("hunter2"));
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_Handshake)

#include "tst_handshake.moc"
