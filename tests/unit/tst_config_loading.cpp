// AI-generated: written by Claude.
#include "akashi/config_store.h"
#include "core/config_loading.h"
#include "core/server_settings.h"

#include <QFile>
#include <QString>
#include <QTest>

namespace tests {
namespace unittests {

class tst_ConfigLoading : public QObject
{
    Q_OBJECT

    typedef QMap<QString, QPair<QString, int>> MusicList;

  private Q_SLOTS:
    void initTestCase();
    void bindIP();
    void charlist();
    void backgrounds();
    void musiclist();
    void ordered_songs();
    void regression_pr_314();
    void iprangeBans();

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
    QPair<QString, int> l_contents;

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

}
}

QTEST_APPLESS_MAIN(tests::unittests::tst_ConfigLoading)

#include "tst_config_loading.moc"
