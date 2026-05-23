// AI-generated: written by Claude.
#include "core/text_filter_registry.h"
#include "fake_packet_context.h"
#include "proto/ic.h"
#include "proto/packet_registry.h"

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
    void doublepostAndBlankpostRules();
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
    l_fields << "Nick" << "1^1" << "5&10" << "0";
    run(Packet("MS", l_fields), l_context);

    QCOMPARE(l_context.broadcast_ic_fields.size(), 23);
    QCOMPARE(l_context.pair_request, 1);
    QCOMPARE(l_context.broadcast_ic_fields.mid(15), QStringList({"Nick", "1^1", "Edgeworth", "thinking", "5&10", "3&4", "1", "0"}));
    QCOMPARE(l_context.current_character_name, QString("Nick"));

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
    l_fields << "" << "1" << "5&10" << "0";
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

    SpeakerContext l_blank;
    l_blank.blankposting_allowed = false;
    QStringList l_empty = baseFields();
    l_empty[4] = "";
    run(Packet("MS", l_empty), l_blank);
    QVERIFY(l_blank.broadcast_ic_fields.isEmpty());
    QCOMPARE(l_blank.calls, QStringList({"message:Blankposting has been forbidden in this area."}));
}

void tst_Ic::textFiltersApplyInOrder()
{
    akashi::TextFilterRegistry l_registry;
    l_registry.registerFilter("gimped", 200,
        [](const QString &) -> std::optional<QString> { return QStringLiteral("I am a heinous criminal."); },
        false, "test");
    l_registry.registerFilter("medieval", 300,
        [](const QString &f_text) -> std::optional<QString> { return "Ye olde " + f_text; },
        false, "test");
    l_registry.registerFilter("disemvoweled", 500,
        [](const QString &f_text) -> std::optional<QString> { return QString(f_text).remove(QRegularExpression("[AEIOUaeiou]")); },
        false, "test");

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
    l_fields << "" << "-1" << "0" << "0";
    l_fields << "0" << "0" << "" << "" << "" << "1" << "";

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
