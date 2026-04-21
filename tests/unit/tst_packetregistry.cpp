// AI-generated: written by Claude.
#include "proto/packet_registry.h"

#include <QTest>

namespace tests {
namespace unittests {

using namespace akashi;

// A handler that does nothing; the tests only care about registration.
class NullHandler : public PacketHandler
{
  public:
    void handle(const Message &, IPacketContext &) const override {}
};

class tst_PacketRegistry : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void registerAndLookUp();
    void duplicateHeaderIsRefused();
    void badRegistrationsAreRefused();
    void unknownHeaderGivesNothing();
    void unregisterAllRemovesOwnersHandlers();
};

void tst_PacketRegistry::registerAndLookUp()
{
    PacketRegistry l_registry;
    const auto l_handler = std::make_shared<NullHandler>();
    QVERIFY(l_registry.registerHandler({"HI", 1, "KICK"}, l_handler));

    const auto l_spec = l_registry.spec("HI");
    QVERIFY(l_spec.has_value());
    QCOMPARE(l_spec->min_args, 1);
    QCOMPARE(l_spec->required_permission, QString("KICK"));
    QCOMPARE(l_registry.handler("HI"), l_handler);
}

void tst_PacketRegistry::duplicateHeaderIsRefused()
{
    PacketRegistry l_registry;
    const auto l_first = std::make_shared<NullHandler>();
    QVERIFY(l_registry.registerHandler({"HI", 1, {}}, l_first));
    QVERIFY(!l_registry.registerHandler({"HI", 2, {}}, std::make_shared<NullHandler>()));

    // The first registration stays untouched.
    QCOMPARE(l_registry.handler("HI"), l_first);
    QCOMPARE(l_registry.spec("HI")->min_args, 1);
}

void tst_PacketRegistry::badRegistrationsAreRefused()
{
    PacketRegistry l_registry;
    QVERIFY(!l_registry.registerHandler({"", 0, {}}, std::make_shared<NullHandler>()));
    QVERIFY(!l_registry.registerHandler({"HI", 0, {}}, nullptr));
    QVERIFY(!l_registry.spec("HI").has_value());
}

void tst_PacketRegistry::unknownHeaderGivesNothing()
{
    PacketRegistry l_registry;
    QVERIFY(!l_registry.spec("MS").has_value());
    QCOMPARE(l_registry.handler("MS"), std::shared_ptr<PacketHandler>());
}

void tst_PacketRegistry::unregisterAllRemovesOwnersHandlers()
{
    PacketRegistry l_registry;
    QVERIFY(l_registry.registerHandler({"HI", 0, {}}, std::make_shared<NullHandler>(), "myplugin"));
    QVERIFY(l_registry.registerHandler({"ID", 0, {}}, std::make_shared<NullHandler>(), "myplugin"));
    QVERIFY(l_registry.registerHandler({"MS", 0, {}}, std::make_shared<NullHandler>(), "core"));

    l_registry.unregisterAll("myplugin");
    QVERIFY(!l_registry.spec("HI").has_value());
    QVERIFY(!l_registry.spec("ID").has_value());
    QVERIFY(l_registry.spec("MS").has_value());

    // The header is free again after its owner is gone.
    QVERIFY(l_registry.registerHandler({"HI", 0, {}}, std::make_shared<NullHandler>()));
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_PacketRegistry)

#include "tst_packetregistry.moc"
