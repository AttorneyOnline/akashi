// AI-generated: written by Claude.
#include "akashi/config_store.h"
#include "core/server_settings.h"
#include "world/config_loading.h"

#include <QFile>
#include <QRegularExpression>
#include <QString>
#include <QTemporaryDir>
#include <QTest>

namespace tests {
namespace unittests {

class tst_ConfigLoading : public QObject
{
    Q_OBJECT

    typedef QMap<QString, std::pair<QString, int>> MusicList;

  private Q_SLOTS:
    void initTestCase();
    void bindIP();
    void charlist();
    void backgrounds();
    void musiclist();
    void ordered_songs();
    void regression_pr_314();
    void iprangeBans();
    void areaRules();
    void areaRulesSkipNameless();
    void areaRulesReadTransformBuckets();
    void deprecatedSettingNamesRemapOrRefuse();
    void malformedAreaRulesFilesLoadNothing();
    void malformedMusicListLoadsNothing();
    void malformedBanFilesWarnAndLoadNothing();

  private:
    akashi::ConfigStore *m_store = nullptr;
};

void tst_ConfigLoading::initTestCase()
{
    m_store = new akashi::ConfigStore("config", this);
    ServerSettings settings(m_store);
    QVERIFY(settings.declare());
}

void tst_ConfigLoading::bindIP()
{
    ServerSettings settings(m_store);
    settings.declare();
    QCOMPARE(settings.bind_ip(), "all");
}

void tst_ConfigLoading::charlist()
{
    QStringList l_characters = akashi::config::loadTextFile(m_store->filePath("characters.txt"));
    QCOMPARE(l_characters.at(0), "Zak");
    QCOMPARE(l_characters.at(1), "Adrian");
    QCOMPARE(l_characters.at(2), "Armstrong");
    QCOMPARE(l_characters.at(3), "Butz");
    QCOMPARE(l_characters.at(4), "Diego");
}

void tst_ConfigLoading::backgrounds()
{
    QStringList l_backgrounds = akashi::config::loadTextFile(m_store->filePath("backgrounds.txt"));

    QCOMPARE(l_backgrounds.at(0), "Anime");
    QCOMPARE(l_backgrounds.at(1), "Zetta");
    QCOMPARE(l_backgrounds.at(2), "default");
    QCOMPARE(l_backgrounds.at(3), "birthday");
    QCOMPARE(l_backgrounds.at(4), "Christmas");
}

void tst_ConfigLoading::musiclist()
{
    akashi::config::MusicCatalog l_catalog = akashi::config::loadMusicList(m_store->filePath("music.json"));
    std::pair<QString, int> l_contents;

    l_contents = l_catalog.songs.value("==Samplelist==");
    QCOMPARE(l_contents.first, "==Samplelist==");
    QCOMPARE(l_contents.second, 0);

    l_contents = l_catalog.songs.value("Announce The Truth (JFA).opus");
    QCOMPARE(l_contents.first, "https://localhost/Announce The Truth (JFA).opus");
    QCOMPARE(l_contents.second, 98);
}

void tst_ConfigLoading::ordered_songs()
{
    akashi::config::MusicCatalog l_catalog = akashi::config::loadMusicList(m_store->filePath("music.json"));
    QCOMPARE(l_catalog.ordered.at(0), "==Samplelist==");
    QCOMPARE(l_catalog.ordered.at(1), "Announce The Truth (AA).opus");
    QCOMPARE(l_catalog.ordered.at(2), "Announce The Truth (AJ).opus");
    QCOMPARE(l_catalog.ordered.at(3), "Announce The Truth (JFA).opus");
}

void tst_ConfigLoading::regression_pr_314()
{
    const QString l_path = m_store->filePath("music.json");
    akashi::config::MusicCatalog l_first = akashi::config::loadMusicList(l_path);
    QCOMPARE(l_first.ordered.isEmpty(), false);
    QCOMPARE(l_first.ordered.size(), 4);

    akashi::config::MusicCatalog l_second = akashi::config::loadMusicList(l_path);
    QCOMPARE(l_first.ordered.size(), l_second.ordered.size());
    QCOMPARE(l_first.ordered, l_second.ordered);
}

void tst_ConfigLoading::iprangeBans()
{
    QStringList l_ipranges = akashi::config::loadIpRangeBans(m_store->filePath("ipbans.json"));
    QCOMPARE(l_ipranges.at(0), "192.0.2.0/24");
    QCOMPARE(l_ipranges.at(1), "198.51.100.0/24");
}

static QString writeAreasFile(QTemporaryDir &f_dir, const QByteArray &f_json)
{
    const QString l_path = f_dir.filePath("areas.json");
    // Fails the test here instead of returning a silent empty path.
    QFile l_file(l_path);
    if (!QTest::qVerify(l_file.open(QIODevice::WriteOnly | QIODevice::Text), "l_file.open(QIODevice::WriteOnly | QIODevice::Text)", qPrintable(l_path), __FILE__, __LINE__)) {
        return {};
    }
    l_file.write(f_json);
    l_file.close();
    return l_path;
}

void tst_ConfigLoading::areaRules()
{
    QTemporaryDir l_dir;
    const QString l_path = writeAreasFile(l_dir, R"({
        "floors": {
            "Courtroom": {
                "rules": {
                    "ic_message_sent": {
                        "before": [{"action": "block", "message": "Silence in the court."}]
                    },
                    "player_joined": {
                        "after": [{"action": "send_message", "message": "All rise.", "target": "player"}]
                    }
                }
            }
        },
        "0:Basement": {
            "background": "gs4",
            "rules": {
                "evidence_added": {
                    "before": [{"action": "check_permission", "permission": "canModifyEvidence"}]
                }
            }
        },
        "1:Hallway": {
            "background": "gs4"
        }
    })");

    const akashi::config::AreaRulesConfig l_config = akashi::config::loadAreaRules(l_path);

    QCOMPARE(l_config.floor_rules.size(), 1);
    const auto l_floor_rules = l_config.floor_rules.value("Courtroom");
    QCOMPARE(l_floor_rules.size(), 2);
    for (const akashi::config::RuleDeclaration &l_rule : l_floor_rules) {
        if (l_rule.event == "ic_message_sent") {
            QCOMPARE(l_rule.phase, akashi::RulePhase::Before);
            QCOMPARE(l_rule.action, QString("block"));
            QCOMPARE(l_rule.args.value("message").toString(), QString("Silence in the court."));
        }
        else {
            QCOMPARE(l_rule.event, QString("player_joined"));
            QCOMPARE(l_rule.phase, akashi::RulePhase::After);
            QCOMPARE(l_rule.action, QString("send_message"));
            QCOMPARE(l_rule.args.value("target").toString(), QString("player"));
        }
    }

    // Only areas that declare rules appear; the key is the area index.
    QCOMPARE(l_config.area_rules.size(), 1);
    const auto l_area_rules = l_config.area_rules.value(0);
    QCOMPARE(l_area_rules.size(), 1);
    QCOMPARE(l_area_rules[0].event, QString("evidence_added"));
    QCOMPARE(l_area_rules[0].phase, akashi::RulePhase::Before);
    QCOMPARE(l_area_rules[0].action, QString("check_permission"));
    QCOMPARE(l_area_rules[0].args.value("permission").toString(), QString("canModifyEvidence"));
    QVERIFY(!l_area_rules[0].args.contains("action"));
}

void tst_ConfigLoading::areaRulesSkipNameless()
{
    QTemporaryDir l_dir;
    const QString l_path = writeAreasFile(l_dir, R"({
        "floors": {
            "Lobby": {
                "rules": {
                    "music_changed": {
                        "before": [{"message": "no action name here"}, {"action": "block"}]
                    }
                }
            }
        }
    })");

    // The skip announces itself so a typo does not vanish silently.
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("names no action and was skipped"));
    const akashi::config::AreaRulesConfig l_config = akashi::config::loadAreaRules(l_path);
    const auto l_rules = l_config.floor_rules.value("Lobby");
    QCOMPARE(l_rules.size(), 1);
    QCOMPARE(l_rules[0].action, QString("block"));
}

void tst_ConfigLoading::areaRulesReadTransformBuckets()
{
    QTemporaryDir l_dir;
    const QString l_path = writeAreasFile(l_dir, R"({
        "floors": {
            "Tavern": {
                "rules": {
                    "ic_message_sent": {
                        "transform": [{"action": "apply_filter", "filter": "medieval"}]
                    }
                }
            }
        }
    })");

    const akashi::config::AreaRulesConfig l_config = akashi::config::loadAreaRules(l_path);
    const auto l_rules = l_config.floor_rules.value("Tavern");
    QCOMPARE(l_rules.size(), 1);
    QCOMPARE(l_rules[0].phase, akashi::RulePhase::Transform);
    QCOMPARE(l_rules[0].action, QString("apply_filter"));
    QCOMPARE(l_rules[0].args.value("filter").toString(), QString("medieval"));
}

void tst_ConfigLoading::deprecatedSettingNamesRemapOrRefuse()
{
    QTemporaryDir l_dir;
    const QString l_path = writeAreasFile(l_dir, R"({
        "floors": {
            "Legacy": {
                "rules": {
                    "music_changed": {
                        "before": [{"action": "check_setting", "setting": "music", "bypass": "gamemaster"}]
                    },
                    "ic_message_sent": {
                        "before": [
                            {"action": "check_setting", "setting": "blankposting"},
                            {"action": "check_setting", "setting": "iniswap"},
                            {"action": "check_setting", "setting": "shouts"},
                            {"action": "check_setting", "setting": "shownames"},
                            {"action": "check_setting", "setting": "ic_messages"}
                        ]
                    },
                    "wtce_used": {
                        "before": [{"action": "check_setting", "setting": "wtce"}]
                    }
                }
            }
        }
    })");

    // The remaps say where the name went; the refusals name the mechanism
    // that replaced them.
    QTest::ignoreMessage(QtInfoMsg, QRegularExpression("\"music\" is deprecated - remapped to \"music_allowed\""));
    QTest::ignoreMessage(QtInfoMsg, QRegularExpression("\"blankposting\" is deprecated - remapped to the check_blankposting action"));
    QTest::ignoreMessage(QtInfoMsg, QRegularExpression("\"iniswap\" is deprecated - remapped to the check_iniswap action"));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("no longer gates \"shouts\".*strip_shouts"));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("no longer gates \"shownames\".*check_showname"));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("no longer gates \"wtce\".*check_wtce"));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("no longer gates \"ic_messages\".*floodguard"));

    const akashi::config::AreaRulesConfig l_config = akashi::config::loadAreaRules(l_path);
    const auto l_rules = l_config.floor_rules.value("Legacy");

    // "music" remaps to the property name and keeps its other arguments,
    // so a save writes the rule back in its new form.
    // "blankposting" and "iniswap" become their dedicated actions.
    // "shouts", "shownames", "wtce" and "ic_messages" refuse outright.
    QCOMPARE(l_rules.size(), 3);
    for (const akashi::config::RuleDeclaration &l_rule : l_rules) {
        if (l_rule.event == "music_changed") {
            QCOMPARE(l_rule.action, QString("check_setting"));
            QCOMPARE(l_rule.args.value("setting").toString(), QString("music_allowed"));
            QCOMPARE(l_rule.args.value("bypass").toString(), QString("gamemaster"));
        }
        else {
            QCOMPARE(l_rule.event, QString("ic_message_sent"));
            QVERIFY(l_rule.action == "check_blankposting" || l_rule.action == "check_iniswap");
            QVERIFY(!l_rule.args.contains("setting"));
        }
    }
}

void tst_ConfigLoading::malformedAreaRulesFilesLoadNothing()
{
    // Broken JSON: the loader warns and answers with an empty config.
    QTemporaryDir l_dir;
    const QString l_broken = writeAreasFile(l_dir, "{ this is not json");
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Unable to load area rules"));
    const akashi::config::AreaRulesConfig l_config = akashi::config::loadAreaRules(l_broken);
    QVERIFY(l_config.floor_rules.isEmpty());
    QVERIFY(l_config.area_rules.isEmpty());

    // Valid JSON that is not an object is refused the same way.
    QTemporaryDir l_array_dir;
    const QString l_array = writeAreasFile(l_array_dir, R"(["not", "an", "object"])");
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Unable to load area rules"));
    const akashi::config::AreaRulesConfig l_array_config = akashi::config::loadAreaRules(l_array);
    QVERIFY(l_array_config.floor_rules.isEmpty());
    QVERIFY(l_array_config.area_rules.isEmpty());

    // A file that does not exist loads nothing and stays quiet.
    const akashi::config::AreaRulesConfig l_missing = akashi::config::loadAreaRules(l_dir.filePath("missing.json"));
    QVERIFY(l_missing.floor_rules.isEmpty());
    QVERIFY(l_missing.area_rules.isEmpty());
}

void tst_ConfigLoading::malformedMusicListLoadsNothing()
{
    QTemporaryDir l_dir;
    const QString l_broken = writeAreasFile(l_dir, "{ this is not json");
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Unable to load musiclist"));
    const akashi::config::MusicCatalog l_catalog = akashi::config::loadMusicList(l_broken);
    QVERIFY(l_catalog.songs.isEmpty());
    QVERIFY(l_catalog.ordered.isEmpty());

    // A missing file warns that it cannot open and loads nothing.
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Unable to open musiclist"));
    const akashi::config::MusicCatalog l_missing = akashi::config::loadMusicList(l_dir.filePath("missing.json"));
    QVERIFY(l_missing.songs.isEmpty());
    QVERIFY(l_missing.ordered.isEmpty());
}

void tst_ConfigLoading::malformedBanFilesWarnAndLoadNothing()
{
    // A broken ipbans.json must warn instead of silently unbanning everyone.
    QTemporaryDir l_dir;
    const QString l_broken = writeAreasFile(l_dir, "{ this is not json");
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Unable to load IP range bans"));
    QVERIFY(akashi::config::loadIpRangeBans(l_broken).isEmpty());

    // Valid JSON that is not an object is refused the same way.
    QTemporaryDir l_array_dir;
    const QString l_array = writeAreasFile(l_array_dir, R"(["not", "an", "object"])");
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Unable to load IP range bans"));
    QVERIFY(akashi::config::loadIpRangeBans(l_array).isEmpty());

    // The same for the ASN ban list, which also skips non-numeric entries.
    QTemporaryDir l_asn_dir;
    const QString l_asn_broken = writeAreasFile(l_asn_dir, "{ this is not json");
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Unable to load banned ASNs"));
    QVERIFY(akashi::config::loadBannedAsns(l_asn_broken).isEmpty());

    QTemporaryDir l_mixed_dir;
    const QString l_mixed = writeAreasFile(l_mixed_dir, R"({"asn": ["64512", "not-a-number"]})");
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("not a number"));
    QCOMPARE(akashi::config::loadBannedAsns(l_mixed), QList<quint32>({64512}));
}

}
}

QTEST_APPLESS_MAIN(tests::unittests::tst_ConfigLoading)

#include "tst_config_loading.moc"
