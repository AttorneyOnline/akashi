#include <QString>
#include <QTest>

#include <include/config_manager.h>
#include <include/data_types.h>

namespace tests {
namespace unittests {

class tst_ConfigManager : public QObject
{

    Q_OBJECT

    typedef QMap<QString, QPair<QString, int>> MusicList;

    using AuthType = DataTypes::AuthType;

  private:
    /**
     * @brief setValue Used to modify values in the test ini file.
     */
    void setValue(QString f_filename, QString f_key, QString f_value);

    /**
     * @brief setValue Used to modify values in the test ini file.
     */
    void setValue(QString f_filename, QString f_key, int f_value);

    /**
     * @brief deleteKey Used to delete a key from the test ini.
     * Tests default loading behaviour for some functions.
     */
    void deleteKey(QString f_filename, QString f_key);

    /**
     * @brief Default path to config.ini
     */
    const QString config = "config/config.ini";

    /**
     * @brief Default path to discord.ini
     */
    const QString discord = "config/discord.ini";

  private slots:

    /**
     * @brief Tests if the config folder is complete. Fails when a config file is missing.
     */
    void verifyServerConfig();

    /**
     * @brief Retrieves the IPs the servers binds to in string format
     */
    void bindIP();

    void charlist();

    void backgrounds();

    void musiclist();

    void ordered_songs();

    /**
     * @brief Tests the loading routine for the musiclist.
     *
     * @details Handles regression testing for a bug fixed in PR#314 in which a
     * musiclist would be loaded twice and the old root instance was not cleared,
     * causing each reload to append the musiclist to the old list.
     */
    void regression_pr_314();

    void CommandInfo();

    void iprangeBans();

    void maxPlayers();

    void serverPort();

    void serverDescription();

    void serverName();

    void motd();

    void webaoEnabled();

    void webaoPort();

    void authType();

    void modpass();

    void logBuffer();

    void loggingType();

    void maxStatements();

    void multiClientLimit();

    void maxCharacters();

    void messageFloodguard();

    void globalMessageFloodguard();

    void assetUrl();

    void diceMaxValue();

    void diceMaxDice();

    void discordWebhookEnabled();

    void discordModcallWebhookEnabled();

    void discordModcallWebhookUrl();

    void discordModcallWebhookContent();

    void discordModcallWebhookSendFile();

    void discordBanWebhookEnabled();

    void discordBanWebhookUrl();

    void discordUptimeEnabled();

    void discordUptimeTime();

    void discordUptimeWebhookUrl();

    void discordWebhookColor();

    void passwordRequirements();

    void passwordMinLength();

    void passwordMaxLength();

    void passwordRequireMixCase();

    void passwordRequireNumbers();

    void passwordRequireSpecialCharacters();

    void passwordCanContainUsername();

    void afkTimeout();

    void magic8BallAnswers();

    void praiseList();

    void reprimandsList();

    void gimpList();

    void cdnList();

    void advertiseServer();

    void advertiserDebug();

    void advertiserIP();

    void advertiserHostname();

    void advertiserCloudflareMode();
};

void tst_ConfigManager::setValue(QString f_filename, QString f_key, QString f_value)
{
    QSettings l_settings(f_filename, QSettings::IniFormat, this);
    l_settings.setIniCodec("UTF-8");
    l_settings.setValue(f_key, f_value);
    l_settings.sync();
}

void tst_ConfigManager::setValue(QString f_filename, QString f_key, int f_value)
{
    QSettings l_settings(f_filename, QSettings::IniFormat, this);
    l_settings.setIniCodec("UTF-8");
    l_settings.setValue(f_key, f_value);
    l_settings.sync();
}

void tst_ConfigManager::deleteKey(QString f_filename, QString f_key)
{
    QSettings l_settings(f_filename, QSettings::IniFormat, this);
    l_settings.setIniCodec("UTF-8");
    l_settings.remove(f_key);
    l_settings.sync();
}

void tst_ConfigManager::verifyServerConfig()
{
    // If the sample folder is not renamed or a file is missing, we fail the test.
    QCOMPARE(ConfigManager::verifyServerConfig(), true);

    // We remove a config file and test again. This should now fail as cdns.txt is missing.
    QCOMPARE(QFile("config/text/cdns.txt").remove(), true);
    QCOMPARE(ConfigManager::verifyServerConfig(), false);

    // We rebuild the file.
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
    setValue(config, "Options/bind_ip", "192.168.0.1");

    QCOMPARE(ConfigManager::bindIP(), "192.168.0.1");
    deleteKey(config, "Options/bind_ip");

    QCOMPARE(ConfigManager::bindIP(), "all");
    setValue(config, "Options/bind_ip", "all");
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

void tst_ConfigManager::musiclist()
{
    MusicList l_musiclist = ConfigManager::musiclist();
    qDebug() << l_musiclist;
    QPair<QString, int> l_contents;
    // The musiclist is structured as {DisplayName , {Songname, Duration}}.

    // Categories have no duration and their alias is always the category name.
    l_contents = l_musiclist.value("==Samplelist==");
    QCOMPARE(l_contents.first, "==Samplelist==");
    QCOMPARE(l_contents.second, 0);

    // The displayname is shown in the musiclist. The realname is what is send to the client when it wants to play the song.
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
    // Populate songlist.
    Q_UNUSED(ConfigManager::musiclist());

    // Tests for regression where a reload of the songlist would cause the list to duplicate.
    QStringList l_list = ConfigManager::ordered_songs();

    // We have a populated ordered list and it has a valid size.
    QCOMPARE(l_list.isEmpty(), false);
    QCOMPARE(l_list.size(), 4);

    // We are reloading the songlist. The size should be the same and the list has not changed.
    Q_UNUSED(ConfigManager::musiclist());
    QCOMPARE(l_list.size(), ConfigManager::ordered_songs().size());
    QCOMPARE(l_list, ConfigManager::ordered_songs());
}

void tst_ConfigManager::CommandInfo()
{
    // Prepare command help cache.
    ConfigManager::loadCommandHelp();
    ConfigManager::help l_help;

    // Load the sample command information.
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
    QCOMPARE(l_ipranges.at(0), "# Test nets");
    QCOMPARE(l_ipranges.at(1), "192.0.2.0/24");
}

void tst_ConfigManager::maxPlayers()
{
    // Load ini-provided value
    QCOMPARE(ConfigManager::maxPlayers(), 100);

    // Set an invalid value. It should return the default value.
    setValue(config, "Options/max_players", "invalid");
    QCOMPARE(ConfigManager::maxPlayers(), 100);

    // Insert an empty-valued key. This should return default.
    setValue(config, "Options/max_players", "");
    QCOMPARE(ConfigManager::maxPlayers(), 100);

    // Key missing, return default
    deleteKey(config, "Options/max_players");
    QCOMPARE(ConfigManager::maxPlayers(), 100);

    // Restore original value
    setValue(config, "Options/max_players", "100");
}

void tst_ConfigManager::serverPort()
{
    QCOMPARE(ConfigManager::serverPort(), 27016);

    setValue(config, "Options/port", "Invalid");
    QCOMPARE(ConfigManager::serverPort(), 27016);

    setValue(config, "Options/port", 10);
    QCOMPARE(ConfigManager::serverPort(), 10);

    deleteKey(config, "Options/port");
    QCOMPARE(ConfigManager::serverPort(), 27016);

    setValue(config, "Options/port", 27016);
}

void tst_ConfigManager::serverDescription()
{
    QCOMPARE(ConfigManager::serverDescription(), "This is a placeholder server description. Tell the world of AO who you are here!");

    setValue(config, "Options/server_description", "");
    QCOMPARE(ConfigManager::serverDescription(), "");

    setValue(config, "Options/server_description", "Sample Description.");
    QCOMPARE(ConfigManager::serverDescription(), "Sample Description.");

    deleteKey(config, "Options/server_description");
    QCOMPARE(ConfigManager::serverDescription(), "This is my flashy new server!");

    setValue(config, "Options/server_description", "This is a placeholder server description. Tell the world of AO who you are here!");
}

void tst_ConfigManager::serverName()
{
    QCOMPARE(ConfigManager::serverName(), "An Unnamed Server");

    setValue(config, "Options/server_name", "");
    QCOMPARE(ConfigManager::serverName(), "");

    setValue(config, "Options/server_name", "1Aa-.,/()Ä");
    QCOMPARE(ConfigManager::serverName(), "1Aa-.,/()Ä");

    deleteKey(config, "Options/server_name");
    QCOMPARE(ConfigManager::serverName(), "An Unnamed Server");

    setValue(config, "Options/server_name", "An Unnamed Server");
}

void tst_ConfigManager::motd()
{
    QCOMPARE(ConfigManager::motd(), "MOTD is not set.");

    setValue(config, "Options/motd", "");
    QCOMPARE(ConfigManager::motd(), "");

    setValue(config, "Options/motd", "This 1s 4 sample MOTD!§$%&/(");
    QCOMPARE(ConfigManager::motd(), "This 1s 4 sample MOTD!§$%&/(");

    deleteKey(config, "Options/motd");
    QCOMPARE(ConfigManager::motd(), "MOTD is not set.");

    setValue(config, "Options/motd", "MOTD is not set.");
}

void tst_ConfigManager::webaoEnabled()
{
    QCOMPARE(ConfigManager::webaoEnabled(), true);

    setValue(config, "Options/webao_enable", "false");
    QCOMPARE(ConfigManager::webaoEnabled(), false);

    setValue(config, "Options/webao_enable", "Invalid");
    QCOMPARE(ConfigManager::webaoEnabled(), true);

    deleteKey(config, "Options/webao_enable");
    QCOMPARE(ConfigManager::webaoEnabled(), false);

    setValue(config, "Options/webao_enable", "true");
}

void tst_ConfigManager::webaoPort()
{
    QCOMPARE(ConfigManager::webaoPort(), 27017);

    setValue(config, "Options/webao_port", 10);
    QCOMPARE(ConfigManager::webaoPort(), 10);

    setValue(config, "Options/webao_port", "Invalid");
    QCOMPARE(ConfigManager::webaoPort(), 27017);

    deleteKey(config, "Options/webao_port");
    QCOMPARE(ConfigManager::webaoPort(), 27017);
}

void tst_ConfigManager::authType()
{
    QCOMPARE(ConfigManager::authType(), AuthType::SIMPLE);

    setValue(config, "Options/auth", "advanced");
    QCOMPARE(ConfigManager::authType(), AuthType::ADVANCED);

    setValue(config, "Options/auth", "Invalid");
    QCOMPARE(ConfigManager::authType(), AuthType::SIMPLE);

    setValue(config, "Options/auth", "simple");
}

void tst_ConfigManager::modpass()
{
    QCOMPARE(ConfigManager::modpass(), "changeme");

    setValue(config, "Options/modpass", "DEADBEEF");
    QCOMPARE(ConfigManager::modpass(), "DEADBEEF");

    setValue(config, "Options/modpass", "changeme");
}

void tst_ConfigManager::logBuffer()
{
    QCOMPARE(ConfigManager::logBuffer(), 500);

    setValue(config, "Options/logbuffer", 100);
    QCOMPARE(ConfigManager::logBuffer(), 100);

    setValue(config, "Options/logbuffer", "Invalid");
    QCOMPARE(ConfigManager::logBuffer(), 500);

    deleteKey(config, "Options/logbuffer");
    QCOMPARE(ConfigManager::logBuffer(), 500);

    setValue(config, "Options/logbuffer", 500);
}

void tst_ConfigManager::loggingType()
{
}

void tst_ConfigManager::maxStatements()
{
}

void tst_ConfigManager::multiClientLimit()
{
}

void tst_ConfigManager::maxCharacters()
{
}

void tst_ConfigManager::messageFloodguard()
{
}

void tst_ConfigManager::globalMessageFloodguard()
{
}

void tst_ConfigManager::assetUrl()
{
}

void tst_ConfigManager::diceMaxValue()
{
}

void tst_ConfigManager::diceMaxDice()
{
}

void tst_ConfigManager::discordWebhookEnabled()
{
}

void tst_ConfigManager::discordModcallWebhookEnabled()
{
}

void tst_ConfigManager::discordModcallWebhookUrl()
{
}

void tst_ConfigManager::discordModcallWebhookContent()
{
}

void tst_ConfigManager::discordModcallWebhookSendFile()
{
}

void tst_ConfigManager::discordBanWebhookEnabled()
{
}

void tst_ConfigManager::discordBanWebhookUrl()
{
}

void tst_ConfigManager::discordUptimeEnabled()
{
}

void tst_ConfigManager::discordUptimeTime()
{
}

void tst_ConfigManager::discordUptimeWebhookUrl()
{
}

void tst_ConfigManager::discordWebhookColor()
{
}

void tst_ConfigManager::passwordRequirements()
{
}

void tst_ConfigManager::passwordMinLength()
{
}

void tst_ConfigManager::passwordMaxLength()
{
}

void tst_ConfigManager::passwordRequireMixCase()
{
}

void tst_ConfigManager::passwordRequireNumbers()
{
}

void tst_ConfigManager::passwordRequireSpecialCharacters()
{
}

void tst_ConfigManager::passwordCanContainUsername()
{
}

void tst_ConfigManager::afkTimeout()
{
}

void tst_ConfigManager::magic8BallAnswers()
{
}

void tst_ConfigManager::praiseList()
{
}

void tst_ConfigManager::reprimandsList()
{
}

void tst_ConfigManager::gimpList()
{
}

void tst_ConfigManager::cdnList()
{
}

void tst_ConfigManager::advertiseServer()
{
}

void tst_ConfigManager::advertiserDebug()
{
}

void tst_ConfigManager::advertiserIP()
{
}

void tst_ConfigManager::advertiserHostname()
{
}

void tst_ConfigManager::advertiserCloudflareMode()
{
}

}
}

QTEST_APPLESS_MAIN(tests::unittests::tst_ConfigManager)

#include "tst_unittest_config_manager.moc"
