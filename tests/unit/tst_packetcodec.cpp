// AI-generated: written by Claude.
#include <QTest>

#include "proto/packet_codec.h"

namespace tests {
namespace unittests {

using namespace akashi;

// A codec that does nothing; the tests only care which instance is chosen.
class MarkerCodec : public Codec
{
  public:
    std::unique_ptr<Message> decode(const Packet &) const override { return nullptr; }
    Packet encode(const Message &) const override { return Packet(); }
};

class tst_PacketCodec : public QObject
{
    Q_OBJECT

  private slots:
    void picksByArchAndVersion();
    void higherPriorityWins();
    void unregisteredHeaderFallsBackToDefault();
    void unregisterAllRemovesOwnersCodecs();
    void combinedRulesApplyAll();
    void dropCodecDecodesNothing();

  private:
    ClientProfile profile(const QString &f_arch, int f_release, int f_major, int f_minor);
};

ClientProfile tst_PacketCodec::profile(const QString &f_arch, int f_release, int f_major, int f_minor)
{
    ClientProfile l_profile;
    l_profile.arch = f_arch;
    l_profile.version = {f_release, f_major, f_minor};
    return l_profile;
}

void tst_PacketCodec::picksByArchAndVersion()
{
    auto l_ms26 = std::make_shared<MarkerCodec>();
    auto l_ms28 = std::make_shared<MarkerCodec>();

    PacketCodecRegistry l_registry;
    l_registry.registerCodec("MS", allOf({archIs("AO2"), versionAtLeast(2, 6, 0)}), 10, l_ms26);
    l_registry.registerCodec("MS", allOf({archIs("AO2"), versionAtLeast(2, 8, 0)}), 20, l_ms28);

    QCOMPARE(l_registry.resolve(profile("AO2", 2, 6, 0)).codecFor("MS"), l_ms26);
    QCOMPARE(l_registry.resolve(profile("AO2", 2, 10, 0)).codecFor("MS"), l_ms28);
    // A webAO client matches neither rule and gets no codec for MS.
    QCOMPARE(l_registry.resolve(profile("webAO", 2, 10, 0)).codecFor("MS"), std::shared_ptr<Codec>());
}

void tst_PacketCodec::higherPriorityWins()
{
    auto l_base = std::make_shared<MarkerCodec>();
    auto l_override = std::make_shared<MarkerCodec>();

    PacketCodecRegistry l_registry;
    l_registry.registerCodec("CT", always(), 0, l_base);
    l_registry.registerCodec("CT", always(), 100, l_override);

    QCOMPARE(l_registry.resolve(profile("AO2", 2, 10, 0)).codecFor("CT"), l_override);
}

void tst_PacketCodec::unregisteredHeaderFallsBackToDefault()
{
    auto l_default = std::make_shared<MarkerCodec>();

    PacketCodecRegistry l_registry;
    l_registry.registerCodec("*", always(), 0, l_default);

    // No codec is registered for CT, so the wildcard default applies.
    QCOMPARE(l_registry.resolve(profile("AO2", 2, 10, 0)).codecFor("CT"), l_default);
}

void tst_PacketCodec::unregisterAllRemovesOwnersCodecs()
{
    auto l_core = std::make_shared<MarkerCodec>();
    auto l_plugin = std::make_shared<MarkerCodec>();

    PacketCodecRegistry l_registry;
    l_registry.registerCodec("MS", always(), 0, l_core, "core");
    l_registry.registerCodec("MS", always(), 100, l_plugin, "myplugin");

    QCOMPARE(l_registry.resolve(profile("AO2", 2, 10, 0)).codecFor("MS"), l_plugin);

    l_registry.unregisterAll("myplugin");
    QCOMPARE(l_registry.resolve(profile("AO2", 2, 10, 0)).codecFor("MS"), l_core);
}

void tst_PacketCodec::combinedRulesApplyAll()
{
    auto l_codec = std::make_shared<MarkerCodec>();

    PacketCodecRegistry l_registry;
    l_registry.registerCodec("MS", allOf({archIs("AO2"), hasFeature("cccc_ic_support")}), 10, l_codec);

    ClientProfile l_without = profile("AO2", 2, 10, 0);
    QCOMPARE(l_registry.resolve(l_without).codecFor("MS"), std::shared_ptr<Codec>());

    ClientProfile l_with = profile("AO2", 2, 10, 0);
    l_with.features.insert("cccc_ic_support");
    QCOMPARE(l_registry.resolve(l_with).codecFor("MS"), l_codec);
}

void tst_PacketCodec::dropCodecDecodesNothing()
{
    // A feature-gated header can register DropCodec below the real codec,
    // so clients outside the feature decode nothing at all.
    DropCodec l_codec;
    QVERIFY(!l_codec.decode(Packet("CT", {"anything"})));
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_PacketCodec)

#include "tst_packetcodec.moc"
