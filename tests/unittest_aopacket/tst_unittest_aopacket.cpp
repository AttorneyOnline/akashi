#include <QObject>
#include <QTest>

#include "include/network/aopacket.h"
#include "include/packet/packet_factory.h"

namespace tests {
namespace unittests {

/**
 * @brief Unit Tester class for the area-related functions.
 */
class Packet : public QObject
{
    Q_OBJECT

  public:
  private slots:
    /**
     * @brief Initializes all tests
     */
    void init();

    /**
     * @brief Creates a packet from a defined header and content.
     */
    void createPacket();

    /**
     * @brief The data function for createPacketFromString();
     */
    void createPacketFromString_data();

    /**
     * @brief Tests the creation of AOPackets from incoming string formatted packets.
     */
    void createPacketFromString();
};

void Packet::init()
{
    AOPacket::registerPackets();
}

void Packet::createPacket()
{
    AOPacket *packet = PacketFactory::createPacket("HI", {"HDID"});
    QCOMPARE(packet->getPacketInfo().header, "HI");
    QCOMPARE(packet->getContent(), {"HDID"});
}

void Packet::createPacketFromString_data()
{
    QTest::addColumn<QString>("incoming_packet");
    QTest::addColumn<QString>("expected_header");
    QTest::addColumn<QStringList>("expected_content");

    QTest::newRow("No Escaped fields") << "HI#1234#"
                                       << "HI"
                                       << QStringList{"1234"};

    QTest::newRow("Multiple fields") << "ID#34#Akashi#"
                                     << "ID"
                                     << QStringList{"34", "Akashi"};

    QTest::newRow("Encoded fields") << "MC#[T<and>T]Objection.opus#0#oldmud0#-1#0#0#"
                                    << "MC"
                                    << QStringList{"[T&T]Objection.opus", "0", "oldmud0", "-1", "0", "0"};

    QTest::newRow("Sequence of encoded characters") << "UNIT#<and><and><percent><num><percent><dollar>#"
                                                    << "UNIT"
                                                    << QStringList{"&&%#%$"};

    QTest::newRow("Unescaped characters") << "MC#20% Cooler#"
                                          << "Unknown"
                                          << QStringList{"Unknown"};

    QTest::newRow("Empty packet") << ""
                                  << "Unknown"
                                  << QStringList{"Unknown"};
}

void Packet::createPacketFromString()
{
    QFETCH(QString, incoming_packet);
    QFETCH(QString, expected_header);
    QFETCH(QStringList, expected_content);

    AOPacket *packet = PacketFactory::createPacket(incoming_packet);
    QCOMPARE(packet->getPacketInfo().header, expected_header);
    QCOMPARE(packet->getContent(), expected_content);
}

}
}

QTEST_APPLESS_MAIN(tests::unittests::Packet)

#include "tst_unittest_aopacket.moc"
