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

    /**
     * @brief Test packet-specific classes
     */
    void createPacketSubclass_data();
    void createPacketSubclass();
};

void Packet::init()
{
    AOPacket::registerPackets();
}

void Packet::createPacketSubclass_data()
{
    QTest::addColumn<QString>("incoming_packet");
    QTest::addColumn<QString>("expected_header");
    QTest::addColumn<int>("expected_minargs");

    QTest::newRow("askchaa") << "askchaa#"
                             << "askchaa"
                             << 0;
    QTest::newRow("CASEA") << "CASEA#"
                           << "CASEA"
                           << 6;
    QTest::newRow("CC") << "CC#"
                        << "CC"
                        << 3;
    QTest::newRow("CH") << "CH#"
                        << "CH"
                        << 1;
    QTest::newRow("CT") << "CT#"
                        << "CT"
                        << 2;
    QTest::newRow("DE") << "DE#"
                        << "DE"
                        << 1;
    QTest::newRow("EE") << "EE#"
                        << "EE"
                        << 4;
    QTest::newRow("GENERIC") << "GENERIC#"
                             << "GENERIC"
                             << 0;
    QTest::newRow("HI") << "HI#"
                        << "HI"
                        << 1;
    QTest::newRow("HP") << "HP#"
                        << "HP"
                        << 2;
    QTest::newRow("ID") << "ID#"
                        << "ID"
                        << 2;
    QTest::newRow("MC") << "MC#"
                        << "MC"
                        << 2;
    QTest::newRow("MS") << "MS#"
                        << "MS"
                        << 15;
    QTest::newRow("PE") << "PE#"
                        << "PE"
                        << 3;
    QTest::newRow("PW") << "PW#"
                        << "PW"
                        << 1;
    QTest::newRow("RC") << "RC#"
                        << "RC"
                        << 0;
    QTest::newRow("RD") << "RD#"
                        << "RD"
                        << 0;
    QTest::newRow("RM") << "RM#"
                        << "RM"
                        << 0;
    QTest::newRow("RT") << "RT#"
                        << "RT"
                        << 1;
    QTest::newRow("SETCASE") << "SETCASE#"
                             << "SETCASE"
                             << 7;
    QTest::newRow("ZZ") << "ZZ#"
                        << "ZZ"
                        << 0;
}

void Packet::createPacketSubclass()
{
    QFETCH(QString, incoming_packet);
    QFETCH(QString, expected_header);
    QFETCH(int, expected_minargs);

    AOPacket *packet = PacketFactory::createPacket(incoming_packet);
    QCOMPARE(packet->getPacketInfo().header, expected_header);
    QCOMPARE(packet->getPacketInfo().min_args, expected_minargs);
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
