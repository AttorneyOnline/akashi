// AI-generated: written by Claude.
#include "core/mmdb_reader.h"

#include <QTemporaryDir>
#include <QTest>

namespace tests {
namespace unittests {

using namespace akashi;

class tst_MmdbReader : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void looksUpTheAsnOfAnAddress();
    void rejectsOtherFiles();

  private:
    QByteArray buildDatabase();
};

// A tiny IPv4 database: 1.0.0.0/8 belongs to ASN 100, 2.0.0.0/8 to ASN 200.
QByteArray tst_MmdbReader::buildDatabase()
{
    const quint32 l_node_count = 9;
    QByteArray l_tree;

    // Each 24 bit node holds two 3 byte records.
    auto addNode = [&l_tree](quint32 f_left, quint32 f_right) {
        l_tree.append(char(f_left >> 16)).append(char(f_left >> 8)).append(char(f_left));
        l_tree.append(char(f_right >> 16)).append(char(f_right >> 8)).append(char(f_right));
    };

    // A map with one number entry, for example {"autonomous_system_number": 100}.
    auto asnEntry = [](quint32 f_asn) {
        QByteArray l_entry;
        l_entry.append(char(0xE1));                                         // map with one pair
        l_entry.append(char(0x40 | 24)).append("autonomous_system_number"); // 24 character key
        l_entry.append(char(0xC1)).append(char(f_asn));                     // one byte number
        return l_entry;
    };

    // Data records sit behind the tree, addressed as node_count + 16 + offset.
    const QByteArray l_first_entry = asnEntry(100);
    const quint32 l_first_pointer = l_node_count + 16;
    const quint32 l_second_pointer = l_first_pointer + l_first_entry.size();

    // Bits 0 to 5 are zero for both networks, bit 6 and 7 tell them apart.
    for (quint32 i = 0; i < 6; i++) {
        addNode(i + 1, l_node_count);
    }
    addNode(7, 8);                           // bit 6: 0 leads to 1.x, 1 leads to 2.x
    addNode(l_node_count, l_first_pointer);  // bit 7 of 1.0.0.0 is 1
    addNode(l_second_pointer, l_node_count); // bit 7 of 2.0.0.0 is 0

    QByteArray l_database = l_tree;
    l_database.append(16, '\0');
    l_database.append(l_first_entry);
    l_database.append(asnEntry(200));

    // The metadata: {"node_count": 9, "record_size": 24, "ip_version": 4}.
    l_database.append(QByteArray("\xab\xcd\xef", 3) + "MaxMind.com");
    l_database.append(char(0xE3));
    l_database.append(char(0x40 | 10)).append("node_count").append(char(0xC1)).append(char(9));
    l_database.append(char(0x40 | 11)).append("record_size").append(char(0xC1)).append(char(24));
    l_database.append(char(0x40 | 10)).append("ip_version").append(char(0xC1)).append(char(4));
    return l_database;
}

void tst_MmdbReader::looksUpTheAsnOfAnAddress()
{
    QTemporaryDir l_dir;
    const QString l_path = l_dir.path() + "/asn.mmdb";
    QFile l_file(l_path);
    QVERIFY(l_file.open(QIODevice::WriteOnly));
    l_file.write(buildDatabase());
    l_file.close();

    MmdbReader l_reader;
    QVERIFY(l_reader.open(l_path));

    QCOMPARE(l_reader.asnForAddress(QHostAddress("1.2.3.4")), 100);
    QCOMPARE(l_reader.asnForAddress(QHostAddress("2.0.0.1")), 200);
    QCOMPARE(l_reader.asnForAddress(QHostAddress("3.0.0.1")), 0);

    // The second lookup of the same address comes from the cache.
    QCOMPARE(l_reader.asnForAddress(QHostAddress("1.2.3.4")), 100);
}

void tst_MmdbReader::rejectsOtherFiles()
{
    QTemporaryDir l_dir;
    const QString l_path = l_dir.path() + "/not_a_database.mmdb";
    QFile l_file(l_path);
    QVERIFY(l_file.open(QIODevice::WriteOnly));
    l_file.write("this is not a database");
    l_file.close();

    MmdbReader l_reader;
    QCOMPARE(l_reader.open(l_path), false);
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_MmdbReader)

#include "tst_mmdbreader.moc"
