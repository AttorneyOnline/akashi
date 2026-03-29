// AI-generated: written by Claude.
#include <QObject>
#include <QTest>

#include "network/aopacket.h"
#include "packet/packet_factory.h"

namespace tests {
namespace unittests {

class Packet : public QObject
{
    Q_OBJECT

  public:
  private Q_SLOTS:
    void init();

    void createPacket();

    void createPacketFromString_data();

    void createPacketFromString();

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

    // The handshake family (HI, ID, askchaa, RC, RM, RD, CC) moved to the
    // packet registry; tst_handshake covers it now.
    QTest::newRow("CASEA") << "CASEA#"
                           << "CASEA"
                           << 6;
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
    QTest::newRow("HP") << "HP#"
                        << "HP"
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
    QTest::newRow("RT") << "RT#"
                        << "RT"
                        << 1;
    QTest::newRow("SETCASE") << "SETCASE#"
                             << "SETCASE"
                             << 7;
    QTest::newRow("ZZ") << "ZZ#"
                        << "ZZ"
                        << 2;
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

    QTest::newRow("Multiple fields") << "ID#34#Akashi#5.0.0#"
                                     << "ID"
                                     << QStringList{"34", "Akashi", "5.0.0"};

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
    QTest::newRow("Bogus Packet - PR 328") << "ZZ#%@%#@^#@&^#@$^@&$^*@&$*@^$&*@$@^$&*@^$&#^&#@$#%"
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

#include "tst_aopacket.moc"
