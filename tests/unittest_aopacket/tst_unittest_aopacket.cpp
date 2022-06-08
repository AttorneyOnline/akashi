#include <QObject>
#include <QTest>

#include "include/network/aopacket.h"

namespace tests {
namespace unittests {

/**
 * @brief Unit Tester class for the area-related functions.
 */
class Packet : public QObject
{
    Q_OBJECT

  public:
    AOPacket m_packet = AOPacket{"", {}};

  private slots:
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

void Packet::createPacket()
{
    AOPacket packet = AOPacket("HI", {"HDID"});
    QCOMPARE(packet.getHeader(), "HI");
    QCOMPARE(packet.getContent(), {"HDID"});
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
                                          << QStringList{"Unknown"}; // This should be impossible.
}

void Packet::createPacketFromString()
{
    QFETCH(QString, incoming_packet);
    QFETCH(QString, expected_header);
    QFETCH(QStringList, expected_content);

    AOPacket packet = AOPacket(incoming_packet);
    QCOMPARE(packet.getHeader(), expected_header);
    QCOMPARE(packet.getContent(), expected_content);
}

}
}

QTEST_APPLESS_MAIN(tests::unittests::Packet)

#include "tst_unittest_aopacket.moc"
