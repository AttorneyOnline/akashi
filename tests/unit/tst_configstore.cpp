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
    void urlChecksAcceptOnlyWebUrls();
    void unknownKeyWarns();
    void reloadSignalsChangedValues();
    void pluginDeclarationsWork();
    void notifierFiresOnReload();
    void notifierSilentWhenUnchanged();
    void settingNotifierIntegration();
    void customFormatPlugin();
    void unknownFormatFallsBackToJson();
    void twoPluginsDifferentFormats();
    void registerFileFormatWithSettingsWrapper();
    void unregisterFormatRemovesEntry();
    void pluginJsonReadFromFile();
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

void tst_ConfigStore::urlChecksAcceptOnlyWebUrls()
{
    const ConfigEntry::Check l_url = url();
    QVERIFY(l_url(QStringLiteral("https://discord.com/api/webhooks/1/token")));
    QVERIFY(l_url(QStringLiteral("http://example.com")));
    QVERIFY(!l_url(QStringLiteral("discord.com/no-scheme")));
    QVERIFY(!l_url(QStringLiteral("file:///etc/passwd")));
    QVERIFY(!l_url(QStringLiteral("https://")));
    QVERIFY(!l_url(QString()));

    // emptyOr lets a setting stay blank without weakening its check.
    const ConfigEntry::Check l_optional = emptyOr(url());
    QVERIFY(l_optional(QString()));
    QVERIFY(l_optional(QStringLiteral("http://example.com")));
    QVERIFY(!l_optional(QStringLiteral("nope")));
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

static bool readIniLike(QIODevice &device, QSettings::SettingsMap &map)
{
    while (!device.atEnd()) {
        QString l_line = QString::fromUtf8(device.readLine()).trimmed();
        if (l_line.isEmpty() || l_line.startsWith('#'))
            continue;
        int l_eq = l_line.indexOf('=');
        if (l_eq < 0)
            continue;
        map.insert(l_line.left(l_eq).trimmed(), l_line.mid(l_eq + 1).trimmed());
    }
    return true;
}

static bool writeIniLike(QIODevice &device, const QSettings::SettingsMap &map)
{
    for (auto it = map.begin(); it != map.end(); ++it) {
        device.write((it.key() + " = " + it.value().toString() + "\n").toUtf8());
    }
    return true;
}

static QSettings::Format testCustomFormat()
{
    static const QSettings::Format fmt =
        QSettings::registerFormat("custom", readIniLike, writeIniLike);
    return fmt;
}

void tst_ConfigStore::customFormatPlugin()
{
    QTemporaryDir l_dir;
    QDir(l_dir.path()).mkpath("plugins");
    writeFile(l_dir.path() + "/plugins/myplugin.custom", "greeting = hi\n");

    ConfigStore l_store(l_dir.path());
    l_store.registerFormat(QStringLiteral("custom"), testCustomFormat(), QStringLiteral("test"));
    QVERIFY(l_store.declarePlugin("myplugin", {{"greeting", QString("hello"), "The greeting."}}, "custom"));
    QCOMPARE(l_store.get<QString>("plugins/myplugin", "greeting"), "hi");
}

void tst_ConfigStore::unknownFormatFallsBackToJson()
{
    QTemporaryDir l_dir;
    QDir(l_dir.path()).mkpath("plugins");
    writeFile(l_dir.path() + "/plugins/fallback.json", R"({"greeting": "from json"})");

    ConfigStore l_store(l_dir.path());
    QTest::ignoreMessage(QtCriticalMsg, QRegularExpression("unknown format.*nope"));
    QVERIFY(l_store.declarePlugin("fallback", {{"greeting", QString("default"), "The greeting."}}, "nope"));
    QCOMPARE(l_store.get<QString>("plugins/fallback", "greeting"), "from json");
}

void tst_ConfigStore::twoPluginsDifferentFormats()
{
    QTemporaryDir l_dir;
    QDir(l_dir.path()).mkpath("plugins");
    writeFile(l_dir.path() + "/plugins/alpha.json", R"({"name": "json-alpha"})");
    writeFile(l_dir.path() + "/plugins/beta.custom", "name = custom-beta\n");

    ConfigStore l_store(l_dir.path());
    l_store.registerFormat(QStringLiteral("custom"), testCustomFormat(), QStringLiteral("test"));

    QVERIFY(l_store.declarePlugin("alpha", {{"name", QString("?"), "Name."}}, "json"));
    QVERIFY(l_store.declarePlugin("beta", {{"name", QString("?"), "Name."}}, "custom"));
    QCOMPARE(l_store.get<QString>("plugins/alpha", "name"), "json-alpha");
    QCOMPARE(l_store.get<QString>("plugins/beta", "name"), "custom-beta");
}

void tst_ConfigStore::registerFileFormatWithSettingsWrapper()
{
    QTemporaryDir l_dir;
    QDir(l_dir.path()).mkpath("plugins");
    writeFile(l_dir.path() + "/plugins/wrapped.custom", "color = red\n");

    ConfigStore l_store(l_dir.path());
    l_store.registerFormat(QStringLiteral("custom"), testCustomFormat(), QStringLiteral("test"));
    l_store.registerFileFormat(QStringLiteral("plugins/wrapped"), QStringLiteral("custom"));

    Settings l_settings(&l_store, "plugins/wrapped");
    Setting<QString> l_color(&l_settings, "color", QString("blue"), "The color.");
    QVERIFY(l_settings.declare());
    QCOMPARE(l_color(), "red");
}

void tst_ConfigStore::unregisterFormatRemovesEntry()
{
    QTemporaryDir l_dir;
    QDir(l_dir.path()).mkpath("plugins");
    ConfigStore l_store(l_dir.path());
    l_store.registerFormat(QStringLiteral("custom"), testCustomFormat(), QStringLiteral("test"));
    l_store.unregisterFormat(QStringLiteral("custom"));

    writeFile(l_dir.path() + "/plugins/gone.json", R"({"val": "ok"})");
    QTest::ignoreMessage(QtCriticalMsg, QRegularExpression("unknown format.*custom"));
    QVERIFY(l_store.declarePlugin("gone", {{"val", QString("x"), "Val."}}, "custom"));
    QCOMPARE(l_store.get<QString>("plugins/gone", "val"), "ok");
}

void tst_ConfigStore::pluginJsonReadFromFile()
{
    QTemporaryDir l_dir;
    QDir(l_dir.path()).mkpath("plugins");
    writeFile(l_dir.path() + "/plugins/testread.json", R"({"greeting": "from file"})");
    QVERIFY(QFileInfo::exists(l_dir.path() + "/plugins/testread.json"));

    ConfigStore l_store(l_dir.path());
    QVERIFY(l_store.declarePlugin("testread", {{"greeting", QString("default"), "The greeting."}}));
    QCOMPARE(l_store.get<QString>("plugins/testread", "greeting"), "from file");
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_ConfigStore)

#include "tst_configstore.moc"
