// AI-generated: written by Claude.
#include "fake_packet_context.h"
#include "proto/moderation.h"
#include "proto/packet_codec.h"
#include "proto/packet_registry.h"

#include <QTest>

namespace tests {
namespace unittests {

using namespace akashi;

class tst_Moderation : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void initTestCase();

    void modcallAlertsTheModerators();
    void modcallNamesItsTarget();
    void modActionNeedsLoginAndPermission();
    void modActionKicksAndBans();

  private:
    void run(const Packet &f_packet, FakeContext &f_context);

    PacketRegistry m_handlers;
    PacketCodecRegistry m_codecs;
};

void tst_Moderation::initTestCase()
{
    registerModerationPackets(m_handlers, m_codecs);
}

void tst_Moderation::run(const Packet &f_packet, FakeContext &f_context)
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

void tst_Moderation::modcallAlertsTheModerators()
{
    FakeContext l_context;
    run(Packet("ZZ", {"Trouble!", "-1"}), l_context);

    QCOMPARE(l_context.moderator_broadcasts.size(), 1);
    QCOMPARE(l_context.moderator_broadcasts.first().header(), QString("ZZ"));
    // No OOC name set, so the character speaks for the caller.
    QCOMPARE(l_context.moderator_broadcasts.first().field(0),
             QString("!!!MODCALL!!!\nArea: Basement\nCaller: [5]Phoenix\nReason: Trouble!"));
    QVERIFY(l_context.calls.contains("recordModcall"));
}

void tst_Moderation::modcallNamesItsTarget()
{
    FakeContext l_context;
    l_context.player_names.insert(2, "Miles");
    run(Packet("ZZ", {"Spam", "2"}), l_context);
    QVERIFY(l_context.moderator_broadcasts.first().field(0).contains("Regarding: Miles\n"));

    // An unknown target leaves the notice without a regarding line.
    FakeContext l_unknown;
    run(Packet("ZZ", {"Spam", "9"}), l_unknown);
    QVERIFY(!l_unknown.moderator_broadcasts.first().field(0).contains("Regarding:"));
}

void tst_Moderation::modActionNeedsLoginAndPermission()
{
    FakeContext l_guest;
    run(Packet("MA", {"3", "0", "spam"}), l_guest);
    QCOMPARE(l_guest.calls, QStringList({"message:You are not logged in!"}));

    FakeContext l_no_kick;
    l_no_kick.authenticated = true;
    run(Packet("MA", {"3", "0", "spam"}), l_no_kick);
    QCOMPARE(l_no_kick.calls, QStringList({"message:You do not have permission to kick users."}));

    FakeContext l_no_ban;
    l_no_ban.authenticated = true;
    l_no_ban.permissions << "kick";
    run(Packet("MA", {"3", "30", "spam"}), l_no_ban);
    QCOMPARE(l_no_ban.calls, QStringList({"message:You do not have permission to ban users."}));
}

void tst_Moderation::modActionKicksAndBans()
{
    FakeContext l_context;
    l_context.authenticated = true;
    l_context.permissions << "kick" << "ban";
    l_context.player_names.insert(3, "Bad");

    run(Packet("MA", {"3", "0", "spamming"}), l_context);
    QCOMPARE(l_context.kicks, QStringList({"3|spamming"}));

    run(Packet("MA", {"3", "30", "spamming"}), l_context);
    QCOMPARE(l_context.bans, QStringList({"3|30|spamming"}));

    // Anything below -1 becomes a permanent ban.
    run(Packet("MA", {"3", "-5", "spamming"}), l_context);
    QCOMPARE(l_context.bans.last(), QString("3|-1|spamming"));

    // A missing target only earns a message.
    run(Packet("MA", {"7", "0", "gone"}), l_context);
    QVERIFY(l_context.calls.contains("message:User not found."));
    QCOMPARE(l_context.kicks.size(), 1);
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_Moderation)

#include "tst_moderation.moc"
