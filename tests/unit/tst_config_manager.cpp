// AI-generated: written by Claude.
#include <QString>
#include <QTest>

#include "akashi/config_store.h"
#include "config_manager.h"

namespace tests {
namespace unittests {

class tst_ConfigManager : public QObject
{

    Q_OBJECT

    typedef QMap<QString, QPair<QString, int>> MusicList;

  private slots:
    void initTestCase();
    void verifyServerConfig();
    void bindIP();
    void charlist();
    void backgrounds();
    void musiclist();
    void ordered_songs();
    void regression_pr_314();
    void CommandInfo();
    void iprangeBans();
};

void tst_ConfigManager::initTestCase()
{
    ConfigManager::setStore(new akashi::ConfigStore("config", this));
}

void tst_ConfigManager::verifyServerConfig()
{
    QCOMPARE(ConfigManager::verifyServerConfig(), true);

    // Removing a config file makes the check fail.
    QCOMPARE(QFile("config/text/cdns.txt").remove(), true);
    QCOMPARE(ConfigManager::verifyServerConfig(), false);

    // Recreate the file for the later tests.
    QFile cdns_config("config/text/cdns.txt");
    if (cdns_config.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream write_stream(&cdns_config);
        write_stream << "cdn.discord.com";
        cdns_config.close();
    }
    else {
        qDebug() << "Unable to recreate cdns config file.";
    }
}

void tst_ConfigManager::bindIP()
{
    QCOMPARE(ConfigManager::bindIP(), "all");
}

void tst_ConfigManager::charlist()
{
    // The list must be unsorted, exactly as defined in the text file.
    QStringList l_characters = ConfigManager::charlist();
    QCOMPARE(l_characters.at(0), "Zak");
    QCOMPARE(l_characters.at(1), "Adrian");
    QCOMPARE(l_characters.at(2), "Armstrong");
    QCOMPARE(l_characters.at(3), "Butz");
    QCOMPARE(l_characters.at(4), "Diego");
}

void tst_ConfigManager::backgrounds()
{
    // The list must be unsorted, exactly as defined in the text file.
    QStringList l_backgrounds = ConfigManager::backgrounds();

    QCOMPARE(l_backgrounds.at(0), "Anime");
    QCOMPARE(l_backgrounds.at(1), "Zetta");
    QCOMPARE(l_backgrounds.at(2), "default");
    QCOMPARE(l_backgrounds.at(3), "birthday");
    QCOMPARE(l_backgrounds.at(4), "Christmas");
}

void tst_ConfigManager::musiclist()
{
    MusicList l_musiclist = ConfigManager::musiclist();
    QPair<QString, int> l_contents;

    // Categories have no duration and their alias is always the category name.
    l_contents = l_musiclist.value("==Samplelist==");
    QCOMPARE(l_contents.first, "==Samplelist==");
    QCOMPARE(l_contents.second, 0);

    // The display name maps to the real name sent to the client and the duration.
    l_contents = l_musiclist.value("Announce The Truth (JFA).opus");
    QCOMPARE(l_contents.first, "https://localhost/Announce The Truth (JFA).opus");
    QCOMPARE(l_contents.second, 98);
}

void tst_ConfigManager::ordered_songs()
{
    QStringList l_ordered_musiclist = ConfigManager::ordered_songs();
    QCOMPARE(l_ordered_musiclist.at(0), "==Samplelist==");
    QCOMPARE(l_ordered_musiclist.at(1), "Announce The Truth (AA).opus");
    QCOMPARE(l_ordered_musiclist.at(2), "Announce The Truth (AJ).opus");
    QCOMPARE(l_ordered_musiclist.at(3), "Announce The Truth (JFA).opus");
}

void tst_ConfigManager::regression_pr_314()
{
    // Reloading the music list must not duplicate its entries.
    Q_UNUSED(ConfigManager::musiclist());

    QStringList l_list = ConfigManager::ordered_songs();
    QCOMPARE(l_list.isEmpty(), false);
    QCOMPARE(l_list.size(), 4);

    Q_UNUSED(ConfigManager::musiclist());
    QCOMPARE(l_list.size(), ConfigManager::ordered_songs().size());
    QCOMPARE(l_list, ConfigManager::ordered_songs());
}

void tst_ConfigManager::CommandInfo()
{
    ConfigManager::loadCommandHelp();
    ConfigManager::help l_help;

    l_help = ConfigManager::commandHelp("foo");
    QCOMPARE(l_help.text, "A sample explanation.");
    QCOMPARE(l_help.usage, "/foo <bar> [baz|qux]");

    l_help = ConfigManager::commandHelp("login");
    QCOMPARE(l_help.text, "Activates the login dialogue to enter your credentials. This command takes no arguments.");
    QCOMPARE(l_help.usage, "/login");
}

void tst_ConfigManager::iprangeBans()
{
    QStringList l_ipranges = ConfigManager::iprangeBans();
    QCOMPARE(l_ipranges.at(0), "192.0.2.0/24");
    QCOMPARE(l_ipranges.at(1), "198.51.100.0/24");
}

}
}

QTEST_APPLESS_MAIN(tests::unittests::tst_ConfigManager)

#include "tst_config_manager.moc"
