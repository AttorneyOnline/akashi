// AI-generated: written by Claude.
#include <QTest>

#include "akashi/config_store.h"
#include "config_manager.h"
#include "fake_packet_context.h"
#include "proto/handshake.h"
#include "proto/packet_codec.h"
#include "proto/packet_registry.h"

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
    void identifyAcceptsAnyVersionButRefusesDouble();
    void resourceCountsMatchTheLists();
    void characterAndMusicListsAreSent();
    void joinSendsTheWholeWorldInOrder();
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
    QVERIFY(ConfigManager::setStore(new akashi::ConfigStore("config", this)));
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
    QCOMPARE(l_context.sent.at(1).fieldCount(), 19);
    QCOMPARE(l_context.sent.at(2).header(), QString("ASS"));
    QCOMPARE(l_context.sent.at(2).field(0), QString("http://attorneyoffline.de/base/"));
}

void tst_Handshake::identifyAcceptsAnyVersionButRefusesDouble()
{
    // A second ID packet is the one protocol error the handler still rejects.
    FakeContext l_double;
    l_double.identified = true;
    run(Packet("ID", {"AO2", "2.10.1"}), l_double);
    QVERIFY(l_double.closed);
    QCOMPARE(l_double.sent.at(0).header(), QString("BD"));

    // A release-1 client (e.g. DRO) is accepted.
    FakeContext l_dro;
    run(Packet("ID", {"AO2", "1.7.5"}), l_dro);
    QVERIFY(!l_dro.closed);
    QVERIFY(l_dro.identified);

    // Even a version that does not parse is accepted; it just defaults to 0.0.0.
    FakeContext l_odd;
    run(Packet("ID", {"AO2", "banana"}), l_odd);
    QVERIFY(!l_odd.closed);
    QVERIFY(l_odd.identified);
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

void tst_Handshake::joinSendsTheWholeWorldInOrder()
{
    FakeContext l_context;
    l_context.stored_hwid = "hwid123";
    l_context.area.def_hp = 7;
    l_context.area.pro_hp = 9;
    l_context.area.background = "gs4";
    l_context.area.side = "def";
    l_context.area.timers = {TimerSnapshot{true, 5000}, TimerSnapshot{false, 0}};

    run(Packet("RD"), l_context);

    QVERIFY(l_context.joined);
    QVERIFY(!l_context.closed);
    const QStringList l_expected = {
        "markJoined", "announceCharsTaken", "sendEvidenceList",
        "send:HP", "send:HP", "send:FA", "send:DONE", "send:BN",
        "message:=== MOTD ===\r\nMOTD is not set.\r\n=============",
        "sendFullArup",
        "send:TI", "send:TI", "send:TI", "send:TI",
        "finishJoin", "broadcastPlayerCount"};
    QCOMPARE(l_context.calls, l_expected);

    QCOMPARE(l_context.sent.at(0).fields(), QStringList({"1", "7"}));
    QCOMPARE(l_context.sent.at(1).fields(), QStringList({"2", "9"}));
    QCOMPARE(l_context.sent.at(4).fields(), QStringList({"gs4", "def"}));
    // The stopped global timer, the running area timer, the stopped one.
    QCOMPARE(l_context.sent.at(5).fields(), QStringList({"0", "3"}));
    QCOMPARE(l_context.sent.at(6).fields(), QStringList({"1", "2"}));
    QCOMPARE(l_context.sent.at(7).fields(), QStringList({"1", "0", "5000"}));
    QCOMPARE(l_context.sent.at(8).fields(), QStringList({"2", "3"}));
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

void tst_Handshake::characterSelectFollowsTheOldRules()
{
    FakeContext l_context;
    l_context.joined = true;
    run(Packet("CC", {"0", "1", "pass"}), l_context);
    QCOMPARE(l_context.selected_char_id, 1);
    QVERIFY(!l_context.closed);

    // A non-numeric choice means spectating.
    FakeContext l_spectator;
    l_spectator.joined = true;
    run(Packet("CC", {"0", "what", "pass"}), l_spectator);
    QCOMPARE(l_spectator.selected_char_id, -1);

    // Out of range closes, but the selection still runs, like it always has.
    FakeContext l_out_of_range;
    l_out_of_range.joined = true;
    run(Packet("CC", {"0", "12", "pass"}), l_out_of_range);
    QVERIFY(l_out_of_range.closed);
    QCOMPARE(l_out_of_range.selected_char_id, 12);

    FakeContext l_not_joined;
    run(Packet("CC", {"0", "1", "pass"}), l_not_joined);
    QCOMPARE(l_not_joined.selected_char_id, -100);
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
