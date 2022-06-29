#include <QTest>

#include <include/config_manager.h>

namespace tests {
namespace unittests {

class tst_ConfigManager : public QObject
{

    Q_OBJECT

    typedef QMap<QString, QPair<QString, int>> MusicList;

  private slots:

    /**
     * @brief Tests if the config folder is complete. Fails when a config file is missing.
     */
    void verifyServerConfig();

    /**
     * @brief Retrieves the IPs the servers binds to in string format
     */
    void bindIP();

    /**
     * @brief Loads a reduced charlist as a QStringList.
     */
    void charlist();

    /**
     * @brief Loads a reduced background list as a QStringList.
     */
    void backgrounds();
};

void tst_ConfigManager::verifyServerConfig()
{
    // If the sample folder is not renamed or a file is missing, we fail the test.
    QCOMPARE(ConfigManager::verifyServerConfig(), true);

    // We remove a config file and test again. This should now fail as cdns.txt is missing.
    qDebug() << QFileInfo(QFile("config/text/cdns.txt")).absoluteFilePath();
    QCOMPARE(QFile("config/text/cdns.txt").remove(), true);
    QCOMPARE(ConfigManager::verifyServerConfig(), false);

    // We rebuild the file.
    QFile cdns_config("config/text/cdns.txt");
    if (cdns_config.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream write_stream(&cdns_config);
        write_stream << "cdn.discord.com";
        cdns_config.close();
        qDebug() << "Recreated cdns config file.";
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
    // We check that the list is unsorted and exactly as defined in the text file.
    QStringList l_characters = ConfigManager::charlist();
    QCOMPARE(l_characters.at(0), "Zak");
    QCOMPARE(l_characters.at(1), "Adrian");
    QCOMPARE(l_characters.at(2), "Armstrong");
    QCOMPARE(l_characters.at(3), "Butz");
    QCOMPARE(l_characters.at(4), "Diego");
}

void tst_ConfigManager::backgrounds()
{
    // We check that the list is unsorted and exactly as defined in the text file.
    QStringList l_backgrounds = ConfigManager::backgrounds();

    QCOMPARE(l_backgrounds.at(0), "Anime");
    QCOMPARE(l_backgrounds.at(1), "Zetta");
    QCOMPARE(l_backgrounds.at(2), "default");
    QCOMPARE(l_backgrounds.at(3), "birthday");
    QCOMPARE(l_backgrounds.at(4), "Christmas");
}

}
}

QTEST_APPLESS_MAIN(tests::unittests::tst_ConfigManager)

#include "tst_unittest_config_manager.moc"
