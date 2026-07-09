// AI-generated: written by Claude.
#include "fake_packet_context.h"
#include "proto/packet_interceptors.h"

#include <QTest>

namespace tests {
namespace unittests {

using namespace akashi;

class tst_PacketInterceptors : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void interceptorsRewriteFields();
    void interceptorsExtendPackets();
    void headerRewriteRetargetsTheChain();
    void dropEndsTheChain();
    void lowerOrderRunsFirst();
    void unregisterAllSweepsAnOwner();
    void unregisterMidFlightFinishesThePacketFirst();
    void emptyChainPassesEverything();
};

void tst_PacketInterceptors::interceptorsRewriteFields()
{
    PacketInterceptors l_chain;
    FakeContext l_context;

    // A header-bound interceptor rewrites a field; an unrelated header
    // never sees it.
    QVERIFY(l_chain.registerInterceptor(QStringLiteral("CT"), 100, [](Packet &f_packet, IPacketContext &) {
        f_packet.setField(1, QStringLiteral("rewritten"));
        return PacketInterceptors::Verdict::Pass;
    }));

    Packet l_ooc(QStringLiteral("CT"), {QStringLiteral("name"), QStringLiteral("original")});
    QVERIFY(l_chain.intercept(l_ooc, l_context));
    QCOMPARE(l_ooc.field(1), QStringLiteral("rewritten"));

    Packet l_other(QStringLiteral("MC"), {QStringLiteral("song"), QStringLiteral("original")});
    QVERIFY(l_chain.intercept(l_other, l_context));
    QCOMPARE(l_other.field(1), QStringLiteral("original"));
}

void tst_PacketInterceptors::interceptorsExtendPackets()
{
    PacketInterceptors l_chain;
    FakeContext l_context;

    // The pairing case: server-side state rides into the outgoing packet as
    // fields, growing it into a richer variant - codecs decode by count.
    l_chain.registerInterceptor(QStringLiteral("MS"), 100, [](Packet &f_packet, IPacketContext &) {
        f_packet.appendField(QStringLiteral("7"));
        f_packet.appendField(QStringLiteral("120&-80"));
        return PacketInterceptors::Verdict::Pass;
    });

    Packet l_packet(QStringLiteral("MS"), {QStringLiteral("chat"), QStringLiteral("-"), QStringLiteral("Phoenix")});
    QVERIFY(l_chain.intercept(l_packet, l_context));
    QCOMPARE(l_packet.fieldCount(), 5);
    QCOMPARE(l_packet.field(3), QStringLiteral("7"));
    QCOMPARE(l_packet.field(4), QStringLiteral("120&-80"));
}

void tst_PacketInterceptors::headerRewriteRetargetsTheChain()
{
    PacketInterceptors l_chain;
    FakeContext l_context;

    // The first interceptor renames the header; the later ones match
    // against the packet as it is by the time they run.
    l_chain.registerInterceptor(QStringLiteral("XX"), 100, [](Packet &f_packet, IPacketContext &) {
        f_packet.setHeader(QStringLiteral("CT"));
        return PacketInterceptors::Verdict::Pass;
    });
    bool l_saw_ct = false;
    l_chain.registerInterceptor(QStringLiteral("CT"), 200, [&l_saw_ct](Packet &, IPacketContext &) {
        l_saw_ct = true;
        return PacketInterceptors::Verdict::Pass;
    });

    Packet l_packet(QStringLiteral("XX"), {QStringLiteral("payload")});
    QVERIFY(l_chain.intercept(l_packet, l_context));
    QCOMPARE(l_packet.header(), QStringLiteral("CT"));
    QVERIFY(l_saw_ct);
}

void tst_PacketInterceptors::dropEndsTheChain()
{
    PacketInterceptors l_chain;
    FakeContext l_context;

    bool l_later_ran = false;
    l_chain.registerInterceptor(QString(), 100, [](Packet &, IPacketContext &) {
        return PacketInterceptors::Verdict::Drop;
    });
    l_chain.registerInterceptor(QString(), 200, [&l_later_ran](Packet &, IPacketContext &) {
        l_later_ran = true;
        return PacketInterceptors::Verdict::Pass;
    });

    Packet l_packet(QStringLiteral("CT"), {QStringLiteral("name"), QStringLiteral("text")});
    QVERIFY(!l_chain.intercept(l_packet, l_context));
    QVERIFY(!l_later_ran);
}

void tst_PacketInterceptors::lowerOrderRunsFirst()
{
    PacketInterceptors l_chain;
    FakeContext l_context;

    QStringList l_ran;
    // Registered out of order on purpose; equal orders keep registration order.
    l_chain.registerInterceptor(QString(), 200, [&l_ran](Packet &, IPacketContext &) { l_ran << "late"; return PacketInterceptors::Verdict::Pass; });
    l_chain.registerInterceptor(QString(), 100, [&l_ran](Packet &, IPacketContext &) { l_ran << "early"; return PacketInterceptors::Verdict::Pass; });
    l_chain.registerInterceptor(QString(), 100, [&l_ran](Packet &, IPacketContext &) { l_ran << "early-second"; return PacketInterceptors::Verdict::Pass; });

    Packet l_packet(QStringLiteral("CT"), {});
    QVERIFY(l_chain.intercept(l_packet, l_context));
    QCOMPARE(l_ran, QStringList({QStringLiteral("early"), QStringLiteral("early-second"), QStringLiteral("late")}));
}

void tst_PacketInterceptors::unregisterAllSweepsAnOwner()
{
    PacketInterceptors l_chain;
    FakeContext l_context;

    bool l_plugin_ran = false;
    bool l_other_ran = false;
    l_chain.registerInterceptor(QString(), 100, [&l_plugin_ran](Packet &, IPacketContext &) { l_plugin_ran = true; return PacketInterceptors::Verdict::Pass; }, QStringLiteral("plugin.x"));
    l_chain.registerInterceptor(QString(), 100, [&l_other_ran](Packet &, IPacketContext &) { l_other_ran = true; return PacketInterceptors::Verdict::Pass; }, QStringLiteral("plugin.y"));
    QCOMPARE(l_chain.size(), 2);

    l_chain.unregisterAll(QStringLiteral("plugin.x"));
    QCOMPARE(l_chain.size(), 1);

    Packet l_packet(QStringLiteral("CT"), {});
    QVERIFY(l_chain.intercept(l_packet, l_context));
    QVERIFY(!l_plugin_ran);
    QVERIFY(l_other_ran);
}

void tst_PacketInterceptors::unregisterMidFlightFinishesThePacketFirst()
{
    PacketInterceptors l_chain;
    FakeContext l_context;

    // An interceptor may unregister its own owner while a packet is in
    // flight (a plugin disabling itself). The running chain finishes on
    // its snapshot; the removal takes effect from the next packet on.
    int l_second_ran = 0;
    l_chain.registerInterceptor(QString(), 100, [&l_chain](Packet &, IPacketContext &) {
        l_chain.unregisterAll(QStringLiteral("plugin.x"));
        return PacketInterceptors::Verdict::Pass; }, QStringLiteral("plugin.x"));
    l_chain.registerInterceptor(QString(), 200, [&l_second_ran](Packet &, IPacketContext &) {
        ++l_second_ran;
        return PacketInterceptors::Verdict::Pass; }, QStringLiteral("plugin.x"));

    Packet l_packet(QStringLiteral("CT"), {QStringLiteral("name"), QStringLiteral("text")});
    QVERIFY(l_chain.intercept(l_packet, l_context));
    QCOMPARE(l_second_ran, 1);
    QCOMPARE(l_chain.size(), 0);

    QVERIFY(l_chain.intercept(l_packet, l_context));
    QCOMPARE(l_second_ran, 1);
}

void tst_PacketInterceptors::emptyChainPassesEverything()
{
    PacketInterceptors l_chain;
    FakeContext l_context;
    Packet l_packet(QStringLiteral("CT"), {QStringLiteral("untouched")});
    QVERIFY(l_chain.intercept(l_packet, l_context));
    QCOMPARE(l_packet.field(0), QStringLiteral("untouched"));
}

} // namespace unittests
} // namespace tests

QTEST_GUILESS_MAIN(tests::unittests::tst_PacketInterceptors)

#include "tst_packet_interceptors.moc"
