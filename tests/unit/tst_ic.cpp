// AI-generated: written by Claude.
#include "core/text_filter_registry.h"
#include "proto/ic.h"
#include "proto/packet_registry.h"
#include "testtools/fake_packet_context.h"

#include <QTest>

namespace tests {
namespace unittests {

using namespace akashi;

// An IC speaker has already joined an area.
class SpeakerContext : public FakeContext
{
  public:
    SpeakerContext()
    {
        joined = true;
        text_filter_registry = &m_default_registry;
    }

  private:
    akashi::TextFilterRegistry m_default_registry;
};

class tst_Ic : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void initTestCase();

    void classicMessageRoundTripsToTheRoom();
    void extendedMessageExpandsPairData();
    void offsetsLoseTheirYForOldClients();
    void rejectsBadValuesAndWrongCharacter();
    void emoteModFourBecomesSix();
    void mutedAndSpectatorsCannotSpeak();
    void floodguardAndAreaGatesStopEverything();
    void beforeRuleBlockRelaysTheReasonOnly();
    void oversizedMessageDropsQuietly();
    void malformedPairAndEffectDataAbortTheBroadcast();
    void pairRequestCommitsOnlyWithAValidMessage();
    void doublepostAndBlankpostRules();
    void evidencePresentationCanBeRefused();
    void transformsRunBetweenGateAndBroadcast();
    void objectionModHygieneRunsBeforeTransforms();
    void transformCanStripTheShout();
    void shownameGateIsABeforeRule();
    void textFiltersApplyInOrder();
    void additiveNeedsTheSameSpeaker();

  private:
    void run(const Packet &f_packet, FakeContext &f_context);
    QStringList baseFields() const;

    PacketRegistry m_handlers;
    PacketCodecRegistry m_codecs;
};

void tst_Ic::initTestCase()
{
    registerIcPackets(m_handlers, m_codecs);
}

void tst_Ic::run(const Packet &f_packet, FakeContext &f_context)
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

QStringList tst_Ic::baseFields() const
{
    return {"chat", "-", "Phoenix", "normal", "Hello there", "def", "1", "0", "0", "0", "0", "0", "0", "0", "0"};
}

void tst_Ic::classicMessageRoundTripsToTheRoom()
{
    SpeakerContext l_context;
    run(Packet("MS", baseFields()), l_context);

    const QStringList l_expected = {"1", "-", "Phoenix", "normal", "Hello there", "def", "1", "0", "0", "0", "0", "0", "0", "0", "0"};
    QCOMPARE(l_context.broadcast_ic_fields, l_expected);
    QCOMPARE(l_context.last_ic_message, QString("Hello there"));
    QCOMPARE(l_context.iniswap, QString("Phoenix"));
    // The testimony recorder always sees the finished message first.
    QVERIFY(l_context.calls.indexOf("applyTestimony") < l_context.calls.indexOf("broadcastIc"));
}

void tst_Ic::extendedMessageExpandsPairData()
{
    SpeakerContext l_context;
    l_context.pair = PairInfo{"Edgeworth", "thinking", "3&4", "1", true};
    QStringList l_fields = baseFields();
    l_fields << "Nick"
             << "1^1"
             << "5&10"
             << "0";
    run(Packet("MS", l_fields), l_context);

    QCOMPARE(l_context.broadcast_ic_fields.size(), 23);
    QCOMPARE(l_context.pair_request, 1);
    QCOMPARE(l_context.broadcast_ic_fields.mid(15), QStringList({"Nick", "1^1", "Edgeworth", "thinking", "5&10", "3&4", "1", "0"}));
    QCOMPARE(l_context.current_character_name, QString("Nick"));
    QCOMPARE(l_context.paired_with, 1);
    // The position commits before the pair resolves against it.
    QVERIFY(l_context.calls.indexOf("updatePosition") < l_context.calls.indexOf("resolvePair"));

    // Without a matching partner the pair collapses.
    SpeakerContext l_alone;
    run(Packet("MS", l_fields), l_alone);
    QCOMPARE(l_alone.broadcast_ic_fields.mid(15), QStringList({"Nick", "-1", "0", "0", "5&10", "0", "0", "0"}));
}

void tst_Ic::offsetsLoseTheirYForOldClients()
{
    SpeakerContext l_context;
    l_context.stored_profile.version = {2, 6, 0};
    l_context.pair = PairInfo{"Edgeworth", "thinking", "3&4", "1", true};
    QStringList l_fields = baseFields();
    l_fields << ""
             << "1"
             << "5&10"
             << "0";
    run(Packet("MS", l_fields), l_context);

    QCOMPARE(l_context.broadcast_ic_fields.at(19), QString("5"));
    QCOMPARE(l_context.broadcast_ic_fields.at(20), QString("3"));
    // The full offset stays recorded for pairing partners on newer clients.
    QCOMPARE(l_context.offset, QString("5&10"));
}

void tst_Ic::rejectsBadValuesAndWrongCharacter()
{
    const QList<QPair<int, QString>> l_bad = {
        {0, "9"},   // unknown desk modifier
        {7, "3"},   // emote modifier outside the allowed set
        {8, "2"},   // a character the sender is not playing
        {11, "5"},  // evidence beyond the area's list
        {12, "2"},  // flip out of range
        {13, "2"},  // realization out of range
        {14, "12"}, // text color out of range
    };
    for (const auto &l_case : l_bad) {
        SpeakerContext l_context;
        QStringList l_fields = baseFields();
        l_fields[l_case.first] = l_case.second;
        run(Packet("MS", l_fields), l_context);
        QVERIFY2(l_context.broadcast_ic_fields.isEmpty(), qPrintable(QString("field %1 = %2 was not rejected").arg(l_case.first).arg(l_case.second)));
        // A rejection never reaches the room or the after-rules.
        QVERIFY(!l_context.calls.contains("broadcastIc"));
        QVERIFY(!l_context.calls.contains("runAfterRule:ic_message_sent"));
        QVERIFY(!l_context.calls.contains("runAfterRule:evidence_presented"));
        // And commits nothing: iniswap, emote and flip wait with the rest
        // of the session state until every check passed.
        QVERIFY(l_context.iniswap.isEmpty());
        QVERIFY(l_context.emote.isEmpty());
        QVERIFY(l_context.flipping.isEmpty());
    }
}

void tst_Ic::emoteModFourBecomesSix()
{
    SpeakerContext l_context;
    QStringList l_fields = baseFields();
    l_fields[7] = "4";
    run(Packet("MS", l_fields), l_context);
    QCOMPARE(l_context.broadcast_ic_fields.at(7), QString("6"));
}

void tst_Ic::mutedAndSpectatorsCannotSpeak()
{
    SpeakerContext l_muted;
    l_muted.ic_chat_allowed = false;
    run(Packet("MS", baseFields()), l_muted);
    QVERIFY(l_muted.broadcast_ic_fields.isEmpty());
    QCOMPARE(l_muted.calls, QStringList({"message:You cannot speak while muted."}));

    SpeakerContext l_spectator;
    l_spectator.spectator = true;
    run(Packet("MS", baseFields()), l_spectator);
    QVERIFY(l_spectator.broadcast_ic_fields.isEmpty());
    QVERIFY(l_spectator.calls.isEmpty());
}

void tst_Ic::floodguardAndAreaGatesStopEverything()
{
    // The floodguard drop is silent, before any rule or side effect.
    SpeakerContext l_flooded;
    l_flooded.ic_message_allowed = false;
    run(Packet("MS", baseFields()), l_flooded);
    QVERIFY(l_flooded.broadcast_ic_fields.isEmpty());
    QCOMPARE(l_flooded.calls, QStringList());
    QVERIFY(l_flooded.last_ic_message.isEmpty());

    // So is the uninvited-in-a-spectatable-area drop.
    SpeakerContext l_uninvited;
    l_uninvited.area_act_allowed = false;
    run(Packet("MS", baseFields()), l_uninvited);
    QVERIFY(l_uninvited.broadcast_ic_fields.isEmpty());
    QCOMPARE(l_uninvited.calls, QStringList());
    QVERIFY(l_uninvited.last_ic_message.isEmpty());
}

void tst_Ic::beforeRuleBlockRelaysTheReasonOnly()
{
    // A blocking before-rule answers with its reason and nothing else: no
    // transform, no broadcast, no after-rule, no doublepost memory.
    SpeakerContext l_context;
    l_context.before_rule_block = "The judge calls for order.";
    run(Packet("MS", baseFields()), l_context);

    QVERIFY(l_context.broadcast_ic_fields.isEmpty());
    QCOMPARE(l_context.calls, QStringList({"checkBeforeRule:ic_message_sent", "message:The judge calls for order."}));
    QVERIFY(l_context.last_ic_message.isEmpty());
}

void tst_Ic::oversizedMessageDropsQuietly()
{
    SpeakerContext l_context;
    QStringList l_fields = baseFields();
    l_fields[4] = QString(l_context.max_message_length + 1, 'a');
    run(Packet("MS", l_fields), l_context);

    // The length check sits right after the gate and nothing commits until
    // every check passed, so an oversized message drops without a trace.
    QVERIFY(l_context.broadcast_ic_fields.isEmpty());
    QCOMPARE(l_context.calls, QStringList({"checkBeforeRule:ic_message_sent"}));
    QVERIFY(l_context.last_ic_message.isEmpty());
    QVERIFY(l_context.iniswap.isEmpty());
    QVERIFY(l_context.emote.isEmpty());
}

void tst_Ic::malformedPairAndEffectDataAbortTheBroadcast()
{
    // An immediate flag outside 0/1 fails the pair validation.
    SpeakerContext l_pair;
    QStringList l_fields = baseFields();
    l_fields << "Nick"
             << "-1"
             << "0"
             << "2";
    run(Packet("MS", l_fields), l_pair);
    QVERIFY(l_pair.broadcast_ic_fields.isEmpty());
    QVERIFY(!l_pair.calls.contains("applyTestimony"));
    QVERIFY(!l_pair.calls.contains("runAfterRule:ic_message_sent"));
    // The abort is clean: showname, offset, position, iniswap, emote, flip,
    // pair request and doublepost memory only commit after every check
    // passed, so a corrected resend of the same text goes through.
    QVERIFY(l_pair.current_character_name.isEmpty());
    QVERIFY(l_pair.offset.isEmpty());
    QVERIFY(!l_pair.calls.contains("updatePosition"));
    QVERIFY(l_pair.last_ic_message.isEmpty());
    QVERIFY(l_pair.iniswap.isEmpty());
    QVERIFY(l_pair.emote.isEmpty());
    QVERIFY(l_pair.flipping.isEmpty());
    QCOMPARE(l_pair.paired_with, -100);
    // A refused message never even resolves a partner.
    QVERIFY(!l_pair.calls.contains("resolvePair"));

    // Effect data outside 0/1 aborts the same way: the incoming packet's
    // sfx_looping, screenshake and additive fields.
    const QList<int> l_effect_fields = {19, 20, 24};
    for (const int l_index : l_effect_fields) {
        SpeakerContext l_effect;
        QStringList l_extended = baseFields();
        l_extended << ""
                   << "-1"
                   << "0"
                   << "0";
        l_extended << "0"
                   << "0"
                   << ""
                   << ""
                   << ""
                   << "0"
                   << "";
        l_extended[l_index] = "2";
        run(Packet("MS", l_extended), l_effect);
        QVERIFY2(l_effect.broadcast_ic_fields.isEmpty(), qPrintable(QString("effect field %1 = 2 was not rejected").arg(l_index)));
        QVERIFY(!l_effect.calls.contains("runAfterRule:ic_message_sent"));
        // An effect abort is just as clean as a pair abort.
        QVERIFY(l_effect.current_character_name.isEmpty());
        QVERIFY(l_effect.last_ic_message.isEmpty());
        QVERIFY(!l_effect.calls.contains("updatePosition"));
        QVERIFY(l_effect.iniswap.isEmpty());
        QVERIFY(l_effect.emote.isEmpty());
        QVERIFY(l_effect.flipping.isEmpty());
        QCOMPARE(l_effect.paired_with, -100);
    }
}

void tst_Ic::pairRequestCommitsOnlyWithAValidMessage()
{
    // A valid pair message commits the request with the rest of the session
    // state, and only then resolves the partner: the resolution reads
    // committed state, position included.
    SpeakerContext l_valid;
    QStringList l_fields = baseFields();
    l_fields << "Nick"
             << "1"
             << "5&10"
             << "0";
    run(Packet("MS", l_fields), l_valid);
    QCOMPARE(l_valid.paired_with, 1);
    QVERIFY(l_valid.calls.indexOf("updatePosition") < l_valid.calls.indexOf("resolvePair"));
    QVERIFY(l_valid.calls.indexOf("setPairingWith") < l_valid.calls.indexOf("resolvePair"));
    QVERIFY(l_valid.calls.indexOf("resolvePair") < l_valid.calls.indexOf("applyTestimony"));

    // A refused pair message keeps the last valid request, so the partner's
    // mutual-pair check still matches against it.
    SpeakerContext l_refused;
    l_refused.paired_with = 1;
    QStringList l_bad = baseFields();
    l_bad << "Nick"
          << "2"
          << "0"
          << "2";
    run(Packet("MS", l_bad), l_refused);
    QVERIFY(l_refused.broadcast_ic_fields.isEmpty());
    QCOMPARE(l_refused.paired_with, 1);
    QVERIFY(!l_refused.calls.contains("setPairingWith"));
    QVERIFY(!l_refused.calls.contains("resolvePair"));
}

void tst_Ic::doublepostAndBlankpostRules()
{
    SpeakerContext l_context;
    l_context.last_ic_message = "Hello there";
    run(Packet("MS", baseFields()), l_context);
    QVERIFY(l_context.broadcast_ic_fields.isEmpty());

    // Testimony jump commands may repeat.
    SpeakerContext l_jump;
    l_jump.last_ic_message = ">3";
    QStringList l_fields = baseFields();
    l_fields[4] = ">3";
    run(Packet("MS", l_fields), l_jump);
    QVERIFY(!l_jump.broadcast_ic_fields.isEmpty());

    // Blankposting is a floor rule now; a rule block stops the message.
    SpeakerContext l_blank;
    l_blank.before_rule_block = "Blankposting has been forbidden in this area.";
    QStringList l_empty = baseFields();
    l_empty[4] = "";
    run(Packet("MS", l_empty), l_blank);
    QVERIFY(l_blank.broadcast_ic_fields.isEmpty());
    QCOMPARE(l_blank.calls, QStringList({"checkBeforeRule:ic_message_sent", "message:Blankposting has been forbidden in this area."}));
}

void tst_Ic::evidencePresentationCanBeRefused()
{
    // A blocked evidence_presented strips the presentation, not the message.
    SpeakerContext l_blocked;
    l_blocked.evidence_total = 3;
    l_blocked.before_rule_block = "Presenting evidence is disabled here.";
    l_blocked.before_rule_block_event = "evidence_presented";
    QStringList l_fields = baseFields();
    l_fields[11] = "2";
    run(Packet("MS", l_fields), l_blocked);
    QCOMPARE(l_blocked.broadcast_ic_fields.at(11), QString("0"));
    QCOMPARE(l_blocked.broadcast_ic_evidence, 0);
    QVERIFY(l_blocked.calls.contains("message:Presenting evidence is disabled here."));
    QVERIFY(!l_blocked.calls.contains("runAfterRule:evidence_presented"));

    // An allowed presentation travels and reacts after the broadcast.
    SpeakerContext l_allowed;
    l_allowed.evidence_total = 3;
    run(Packet("MS", l_fields), l_allowed);
    QCOMPARE(l_allowed.broadcast_ic_evidence, 2);
    QVERIFY(l_allowed.calls.indexOf("broadcastIc") < l_allowed.calls.indexOf("runAfterRule:evidence_presented"));

    // No evidence attached means the event never fires.
    SpeakerContext l_plain;
    run(Packet("MS", baseFields()), l_plain);
    QVERIFY(!l_plain.calls.contains("checkBeforeRule:evidence_presented"));
}

void tst_Ic::transformsRunBetweenGateAndBroadcast()
{
    SpeakerContext l_context;
    run(Packet("MS", baseFields()), l_context);

    // The gate first, the transforms once, the broadcast last.
    QCOMPARE(l_context.calls.count("runTransformRules:ic_message_sent"), 1);
    QVERIFY(l_context.calls.indexOf("checkBeforeRule:ic_message_sent") < l_context.calls.indexOf("runTransformRules:ic_message_sent"));
    QVERIFY(l_context.calls.indexOf("runTransformRules:ic_message_sent") < l_context.calls.indexOf("broadcastIc"));

    // Both dispatches carry the full payload rules read.
    for (const QVariantMap &l_payload : {l_context.last_before_payload, l_context.last_transform_payload}) {
        QCOMPARE(l_payload.value("message").toString(), QString("Hello there"));
        QCOMPARE(l_payload.value("char_name").toString(), QString("Phoenix"));
        QCOMPARE(l_payload.value("objection_mod").toString(), QString("0"));
        QVERIFY(l_payload.contains("showname"));
        QCOMPARE(l_payload.value("evidence").toInt(), 0);
    }

    // A transform's rewrite of the message reaches the room.
    SpeakerContext l_rewritten;
    l_rewritten.transform_result = {{QStringLiteral("message"), QStringLiteral("Ye olde Hello there")}};
    run(Packet("MS", baseFields()), l_rewritten);
    QCOMPARE(l_rewritten.broadcast_ic_fields.at(4), QString("Ye olde Hello there"));
}

void tst_Ic::objectionModHygieneRunsBeforeTransforms()
{
    // Out-of-range objection modifiers are protocol hygiene and drop the
    // message regardless of any area rule.
    for (const QString &l_bad : {QString("9"), QString("-1")}) {
        SpeakerContext l_context;
        QStringList l_fields = baseFields();
        l_fields[10] = l_bad;
        run(Packet("MS", l_fields), l_context);
        QVERIFY2(l_context.broadcast_ic_fields.isEmpty(), qPrintable(QString("objection_mod %1 was not rejected").arg(l_bad)));
        QVERIFY(!l_context.calls.contains("runTransformRules:ic_message_sent"));
    }

    // A custom shout carries text metadata and passes through untouched.
    SpeakerContext l_custom;
    QStringList l_fields = baseFields();
    l_fields[10] = "4&Gotcha";
    run(Packet("MS", l_fields), l_custom);
    QCOMPARE(l_custom.broadcast_ic_fields.at(10), QString("4&Gotcha"));
}

void tst_Ic::transformCanStripTheShout()
{
    // The shout downgrade is the strip_shouts transform now; the handler
    // just reads the rewritten objection_mod back.
    SpeakerContext l_context;
    l_context.transform_result = {{QStringLiteral("objection_mod"), QStringLiteral("0")}};
    QStringList l_fields = baseFields();
    l_fields[10] = "2";
    run(Packet("MS", l_fields), l_context);

    QCOMPARE(l_context.broadcast_ic_fields.at(10), QString("0"));
    QCOMPARE(l_context.last_transform_payload.value("objection_mod").toString(), QString("2"));
}

void tst_Ic::shownameGateIsABeforeRule()
{
    // The area's showname policy is the check_showname before-rule; the
    // dispatch carries the showname it decides on.
    SpeakerContext l_blocked;
    l_blocked.before_rule_block = "Shownames are not allowed in this area!";
    QStringList l_fields = baseFields();
    l_fields << "Nick"
             << "-1"
             << "0"
             << "0";
    run(Packet("MS", l_fields), l_blocked);

    QVERIFY(l_blocked.broadcast_ic_fields.isEmpty());
    QVERIFY(l_blocked.calls.contains("message:Shownames are not allowed in this area!"));
    QCOMPARE(l_blocked.last_before_payload.value("showname").toString(), QString("Nick"));

    // The length hygiene stays with the handler.
    SpeakerContext l_long;
    QStringList l_long_fields = baseFields();
    l_long_fields << QString(31, 'a')
                  << "-1"
                  << "0"
                  << "0";
    run(Packet("MS", l_long_fields), l_long);
    QVERIFY(l_long.broadcast_ic_fields.isEmpty());
    QVERIFY(l_long.calls.contains("message:Your showname is too long! Please limit it to under 30 characters"));
    // Like the rule-based rejection above, the length hygiene leaves the
    // doublepost memory and the showname alone, so a corrected resend of
    // the same text goes through.
    QVERIFY(l_long.last_ic_message.isEmpty());
    QVERIFY(!l_long.calls.contains("setCharacterName"));
}

void tst_Ic::textFiltersApplyInOrder()
{
    akashi::TextFilterRegistry l_registry;
    l_registry.registerFilter(
        "gimped", 200, [](const QString &) -> std::optional<QString> { return QStringLiteral("I am a heinous criminal."); }, false, "test");
    l_registry.registerFilter(
        "medieval", 300, [](const QString &f_text) -> std::optional<QString> { return "Ye olde " + f_text; }, false, "test");
    l_registry.registerFilter(
        "disemvoweled", 500, [](const QString &f_text) -> std::optional<QString> {
            static const QRegularExpression s_vowels("[AEIOUaeiou]");
            return QString(f_text).remove(s_vowels); }, false, "test");

    SpeakerContext l_context;
    l_context.text_filter_registry = &l_registry;
    l_context.active_filter_ids = {"gimped", "medieval", "disemvoweled"};
    run(Packet("MS", baseFields()), l_context);

    // Gimp replaces, medieval wraps, disemvowel strips, in that order.
    QCOMPARE(l_context.broadcast_ic_fields.at(4), QString("Y ld  m  hns crmnl."));
}

void tst_Ic::additiveNeedsTheSameSpeaker()
{
    QStringList l_fields = baseFields();
    l_fields << ""
             << "-1"
             << "0"
             << "0";
    l_fields << "0"
             << "0"
             << ""
             << ""
             << ""
             << "1"
             << "";

    // A different last speaker cancels the additive flag.
    SpeakerContext l_other;
    l_other.last_area_message = baseFields();
    l_other.last_area_message[8] = "2";
    run(Packet("MS", l_fields), l_other);
    QCOMPARE(l_other.broadcast_ic_fields.at(28), QString("0"));

    // The same speaker keeps it, and the text continues with a space.
    SpeakerContext l_same;
    l_same.last_area_message = baseFields();
    run(Packet("MS", l_fields), l_same);
    QCOMPARE(l_same.broadcast_ic_fields.at(28), QString("1"));
    QCOMPARE(l_same.broadcast_ic_fields.at(4), QString(" Hello there"));
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_Ic)

#include "tst_ic.moc"
