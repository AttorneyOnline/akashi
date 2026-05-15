// AI-generated: written by Claude.
#include "akashi/config_store.h"
#include "akashi/setting_notifier.h"
#include "akashi/settings.h"

#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace tests {
namespace unittests {

using namespace akashi;

class tst_ConfigStore : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void defaultsFillMissingValues();
    void stringValuesConvertToDeclaredTypes();
    void badValueFailsDeclare();
    void failedCheckFailsDeclare();
    void combinedChecksApplyAll();
    void unknownKeyWarns();
    void reloadSignalsChangedValues();
    void pluginDeclarationsWork();
    void notifierFiresOnReload();
    void notifierSilentWhenUnchanged();
    void settingNotifierIntegration();
};

static void writeFile(const QString &f_path, const QByteArray &f_content)
{
    QFile l_file(f_path);
    QVERIFY(l_file.open(QIODevice::WriteOnly));
    l_file.write(f_content);
}

// QSettings::sync() skips re-reading when the filesystem timestamp has not
// changed. On NTFS the write timestamp can stay the same for two writes
// that land within the same coalescing window. This helper rewrites the
// file until the mtime actually advances.
static void writeFileAndWaitForMtime(const QString &f_path, const QByteArray &f_content)
{
    const QDateTime l_before = QFileInfo(f_path).lastModified();
    for (int i = 0; i < 200; i++) {
        writeFile(f_path, f_content);
        if (QFileInfo(f_path).lastModified() > l_before)
            return;
        QTest::qWait(10);
    }
    QVERIFY2(false, "mtime did not advance");
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

void tst_ConfigStore::combinedChecksApplyAll()
{
    QTemporaryDir l_dir;
    writeFile(l_dir.path() + "/config.json", R"({"Options": {"port": 70000}})");

    ConfigStore l_store(l_dir.path());
    const ConfigEntry::Check l_check = allOf({atLeast(1), atMost(65535)});
    QCOMPARE(l_store.declare("config", {{"Options/port", 27016, "The port.", l_check}}), false);

    writeFile(l_dir.path() + "/config.json", R"({"Options": {"port": 8080}})");
    ConfigStore l_second_store(l_dir.path());
    QVERIFY(l_second_store.declare("config", {{"Options/port", 27016, "The port.", l_check}}));
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
    const QString l_path = l_dir.path() + "/config.json";
    writeFile(l_path, R"({"Options": {"motd": "old"}})");

    ConfigStore l_store(l_dir.path());
    QVERIFY(l_store.declare("config", {{"Options/motd", QString("none"), "The message of the day."}}));

    writeFileAndWaitForMtime(l_path, R"({"Options": {"motd": "new"}})");

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

void tst_ConfigStore::notifierFiresOnReload()
{
    QTemporaryDir l_dir;
    const QString l_path = l_dir.path() + "/config.json";
    writeFile(l_path, R"({"Options": {"motd": "old"}})");

    ConfigStore l_store(l_dir.path());
    QVERIFY(l_store.declare("config", {{"Options/motd", QString("none"), "The message of the day."}}));

    SettingNotifier *l_notifier = l_store.notifier("config", "Options/motd");
    QVERIFY(l_notifier);
    QSignalSpy l_spy(l_notifier, &SettingNotifier::changed);

    writeFileAndWaitForMtime(l_path, R"({"Options": {"motd": "new"}})");
    l_store.reload();

    QCOMPARE(l_spy.count(), 1);
    QCOMPARE(l_store.get<QString>("config", "Options/motd"), "new");
}

void tst_ConfigStore::notifierSilentWhenUnchanged()
{
    QTemporaryDir l_dir;
    const QString l_path = l_dir.path() + "/config.json";
    writeFile(l_path, R"({"Options": {"motd": "same"}})");

    ConfigStore l_store(l_dir.path());
    QVERIFY(l_store.declare("config", {{"Options/motd", QString("none"), "The message of the day."}}));

    SettingNotifier *l_notifier = l_store.notifier("config", "Options/motd");
    QSignalSpy l_spy(l_notifier, &SettingNotifier::changed);

    // Rewrite with the same value — notifier must stay silent.
    writeFileAndWaitForMtime(l_path, R"({"Options": {"motd": "same"}})");
    l_store.reload();

    QCOMPARE(l_spy.count(), 0);
}

void tst_ConfigStore::settingNotifierIntegration()
{
    QTemporaryDir l_dir;
    const QString l_path = l_dir.path() + "/config.json";
    writeFile(l_path, R"({"Options": {"motd": "hello"}})");

    ConfigStore l_store(l_dir.path());
    Settings l_settings(&l_store, "config");
    Setting<QString> l_motd(&l_settings, "Options/motd", QString("default"), "The message of the day.");
    QVERIFY(l_settings.declare());

    QCOMPARE(l_motd(), "hello");

    SettingNotifier *l_notifier = l_motd.notifier();
    QVERIFY(l_notifier);
    QSignalSpy l_spy(l_notifier, &SettingNotifier::changed);

    writeFileAndWaitForMtime(l_path, R"({"Options": {"motd": "world"}})");
    l_store.reload();

    QCOMPARE(l_spy.count(), 1);
    QCOMPARE(l_motd(), "world");
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_ConfigStore)

#include "tst_configstore.moc"
