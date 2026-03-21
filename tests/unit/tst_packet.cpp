// AI-generated: written by Claude.
#include <QTest>

#include "proto/packet.h"

namespace tests {
namespace unittests {

using namespace akashi;

class tst_Packet : public QObject
{
    Q_OBJECT

  private slots:
    void parse_data();
    void parse();
    void parseRejectsBrokenInput_data();
    void parseRejectsBrokenInput();
    void serializeMatchesTheWireFormat();
    void serializeDoesNotChangeThePacket();
    void evidenceKeepsItsSeparator();
};

void tst_Packet::parse_data()
{
    QTest::addColumn<QString>("raw");
    QTest::addColumn<QString>("header");
    QTest::addColumn<QStringList>("fields");

    QTest::newRow("Single field") << "HI#1234#"
                                  << "HI" << QStringList{"1234"};
    QTest::newRow("Multiple fields") << "ID#34#Akashi#5.0.0#"
                                     << "ID" << QStringList{"34", "Akashi", "5.0.0"};
    QTest::newRow("Encoded fields") << "MC#[T<and>T]Objection.opus#0#oldmud0#-1#0#0#"
                                    << "MC" << QStringList{"[T&T]Objection.opus", "0", "oldmud0", "-1", "0", "0"};
    QTest::newRow("Encoded sequence") << "UNIT#<and><and><percent><num><percent><dollar>#"
                                      << "UNIT" << QStringList{"&&%#%$"};
}

void tst_Packet::parse()
{
    QFETCH(QString, raw);
    QFETCH(QString, header);
    QFETCH(QStringList, fields);

    const Packet l_packet = Packet::parse(raw);
    QCOMPARE(l_packet.isNull(), false);
    QCOMPARE(l_packet.header(), header);
    QCOMPARE(l_packet.fields(), fields);
}

void tst_Packet::parseRejectsBrokenInput_data()
{
    QTest::addColumn<QString>("raw");

    QTest::newRow("Empty input") << "";
    QTest::newRow("Unescaped percent") << "MC#20% Cooler#";
    QTest::newRow("Fantacrypt leftovers") << "#F%D%E#";
    QTest::newRow("No header") << "#foo#";
}

void tst_Packet::parseRejectsBrokenInput()
{
    QFETCH(QString, raw);
    QCOMPARE(Packet::parse(raw).isNull(), true);
}

void tst_Packet::serializeMatchesTheWireFormat()
{
    QCOMPARE(Packet("DONE").serialize(), "DONE##%");
    QCOMPARE(Packet("HP", {"1", "10"}).serialize(), "HP#1#10#%");
    QCOMPARE(Packet("CT", {"Tester", "a#b%c$d&e"}).serialize(), "CT#Tester#a<num>b<percent>c<dollar>d<and>e#%");
}

void tst_Packet::serializeDoesNotChangeThePacket()
{
    const Packet l_packet("CT", {"Tester", "100% real"});
    QCOMPARE(l_packet.serialize(), l_packet.serialize());
    QCOMPARE(l_packet.field(1), "100% real");
}

void tst_Packet::evidenceKeepsItsSeparator()
{
    QCOMPARE(Packet("LE", {"name&description&image"}).serialize(), "LE#name&description&image#%");
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_Packet)

#include "tst_packet.moc"
