// AI-generated: written by Claude.
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "akashi/config_store.h"

namespace tests {
namespace unittests {

using namespace akashi;

class tst_ConfigStore : public QObject
{
    Q_OBJECT

  private slots:
    void defaultsFillMissingValues();
    void stringValuesConvertToDeclaredTypes();
    void badValueFailsDeclare();
    void failedCheckFailsDeclare();
    void unknownKeyWarns();
    void reloadSignalsChangedValues();
    void pluginDeclarationsWork();
};

static void writeFile(const QString &f_path, const QByteArray &f_content)
{
    QFile l_file(f_path);
    QVERIFY(l_file.open(QIODevice::WriteOnly));
    l_file.write(f_content);
}

void tst_ConfigStore::defaultsFillMissingValues()
{
    QTemporaryDir l_dir;
    ConfigStore l_store(l_dir.path());
    QVERIFY(l_store.declare("config", {{"Options/port", 27016, "The port."}}));
    QCOMPARE(l_store.get<int>("config", "Options/port"), 27016);
}

void tst_ConfigStore::stringValuesConvertToDeclaredTypes()
{
    QTemporaryDir l_dir;
    writeFile(l_dir.path() + "/config.json", R"({"Options": {"port": "5000", "advertise": "true"}})");

    ConfigStore l_store(l_dir.path());
    QVERIFY(l_store.declare("config", {{"Options/port", 27016, "The port."}, {"Options/advertise", false, "Advertise."}}));
    QCOMPARE(l_store.get<int>("config", "Options/port"), 5000);
    QCOMPARE(l_store.get<bool>("config", "Options/advertise"), true);
}

void tst_ConfigStore::badValueFailsDeclare()
{
    QTemporaryDir l_dir;
    writeFile(l_dir.path() + "/config.json", R"({"Options": {"port": "banana"}})");

    ConfigStore l_store(l_dir.path());
    QCOMPARE(l_store.declare("config", {{"Options/port", 27016, "The port."}}), false);
}

void tst_ConfigStore::failedCheckFailsDeclare()
{
    QTemporaryDir l_dir;
    writeFile(l_dir.path() + "/config.json", R"({"Options": {"port": 999999}})");

    ConfigStore l_store(l_dir.path());
    QCOMPARE(l_store.declare("config", {{"Options/port", 27016, "The port.", inRange(1, 65535)}}), false);
}

void tst_ConfigStore::unknownKeyWarns()
{
    QTemporaryDir l_dir;
    writeFile(l_dir.path() + "/config.json", R"({"Options": {"prot": 5000}})");

    ConfigStore l_store(l_dir.path());
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("unknown setting.*Options/prot"));
    QVERIFY(l_store.declare("config", {{"Options/port", 27016, "The port."}}));
}

void tst_ConfigStore::reloadSignalsChangedValues()
{
    QTemporaryDir l_dir;
    writeFile(l_dir.path() + "/config.json", R"({"Options": {"motd": "old"}})");

    ConfigStore l_store(l_dir.path());
    QVERIFY(l_store.declare("config", {{"Options/motd", QString("none"), "The message of the day."}}));

    writeFile(l_dir.path() + "/config.json", R"({"Options": {"motd": "new"}})");
    QSignalSpy l_spy(&l_store, &ConfigStore::valueChanged);
    l_store.reload();

    QCOMPARE(l_spy.count(), 1);
    QCOMPARE(l_spy.at(0).at(1).toString(), "Options/motd");
    QCOMPARE(l_store.get<QString>("config", "Options/motd"), "new");
}

void tst_ConfigStore::pluginDeclarationsWork()
{
    QTemporaryDir l_dir;
    ConfigStore l_store(l_dir.path());
    QVERIFY(l_store.declarePlugin("myplugin", {{"greeting", QString("hello"), "The greeting."}}));
    QCOMPARE(l_store.get<QString>("plugins/myplugin", "greeting"), "hello");
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_ConfigStore)

#include "tst_configstore.moc"
