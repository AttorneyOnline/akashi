#include <QTest>

#include <include/music_manager.h>

namespace tests {
namespace unittests {

/**
 * @brief Unit Tester class for the musiclist-related functions.
 */
class MusicListManager : public QObject
{
    Q_OBJECT
public :

    MusicManager* m_music_manager;

private slots:
    /**
    * @brief Initialises every tests with creating a new MusicManager with a small sample root list.
    */
    void init();

    /**
     * @brief Tests the registration of areas in the music manager.
     */
    void registerArea();

    /**
     * @brief Tests toggling the enabling/disabling of the prepend behaviour of our root list.
     */
    void toggleRootEnabled();

    /**
     * @brief The data function for validateSong()
     */
    void validateSong_data();

    /**
     * @brief Tests validation of song candidates.
     */
    void validateSong();

    /**
     * @brief Tests the addition of custom music.
     */
    void addCustomSong();

    /**
     * @brief Tests the addition of a custom category.
     */
    void addCustomCategory();

    /**
     * @brief Test the sanitisation of the custom list when root prepend is reenabled.
     */
    void sanitiseCustomList();

    /**
     * @brief Tests the removing of custom songs and categories.
     */
    void removeCategorySong();

    /**
     * @brief Tests the retrieval of song information.
     */
    void songInformation();

    /**
     * @brief Tests the retrieval of the full musiclist for an area.
     */
    void musiclist();


};

void MusicListManager::init()
{
    QMap<QString,QPair<QString,int>> l_test_list;
    l_test_list.insert("==Music==",{"==Music==",0});
    l_test_list.insert("Announce The Truth (AJ).opus",{"Announce The Truth (AJ).opus",59});
    l_test_list.insert("Announce The Truth (JFA).opus",{"Announce The Truth (JFA).opus",98});

    QStringList l_list = {};
    l_list << "==Music==" << "Announce The Truth (AJ).opus" << "Announce The Truth (JFA).opus";

    m_music_manager = new MusicManager(nullptr, l_list ,{"my.cdn.com","your.cdn.com"}, l_test_list);
}

void MusicListManager::registerArea()
{
    {
        //We register a single area with the music manager of ID 0.
        //Creation should work as there are no other areas yet.
        bool l_creation_success = m_music_manager->registerArea(0);
        QCOMPARE(l_creation_success,true);
    }
    {
        //Someone tries to register the same area again!
        //This should fail as this area already exists.
        bool l_creation_success = m_music_manager->registerArea(0);
        QCOMPARE(l_creation_success,false);
    }
}

void MusicListManager::toggleRootEnabled()
{
    {
        //We register an area of ID0 and toggle the inclusion of global list.
        m_music_manager->registerArea(0);
        QCOMPARE(m_music_manager->toggleRootEnabled(0), false);
    }
    {
        //We toggle it again. It should return true now.
        //Since this is now true, we should have the root list with customs cleared.
        QCOMPARE(m_music_manager->toggleRootEnabled(0), true);
    }
}

void MusicListManager::validateSong_data()
{
    //Songname can also be the realname.
    QTest::addColumn<QString>("songname");
    QTest::addColumn<bool>("expectedResult");

    QTest::addRow("Songname - No extension") << "Announce The Truth (AA)" << false;
    QTest::addRow("Songname - Valid Extension") << "Announce The Truth (AA).opus" << true;
    QTest::addRow("Songname - Invalid Extension") << "Announce The Truth (AA).aac" << false;
    QTest::addRow("URL - Valid primary") << "https://my.cdn.com/mysong.opus" << true;
    QTest::addRow("URL - Valid secondary") << "https://your.cdn.com/mysong.opus" << true;
    QTest::addRow("URL - Invalid extension") << "https://my.cdn.com/mysong.aac." << false;
    QTest::addRow("URL - Invalid prefix") << "ftp://my.cdn.com/mysong.opus" << false;
    QTest::addRow("URL - Invalid missing prefix") << "my.cdn.com/mysong.opus" << false;
    QTest::addRow("URL - Invalid CDN") << "https://myipgrabber.com/mysong.opus" << false;
    QTest::addRow("URL - Subdomain Attack") << "https://my.cdn.com.fakedomain.com/mysong.opus" << false;
}

void MusicListManager::validateSong()
{
    QFETCH(QString,songname);
    QFETCH(bool,expectedResult);

    bool l_result = m_music_manager->validateSong(songname, {"my.cdn.com","your.cdn.com"});
    QCOMPARE(expectedResult,l_result);
}

void MusicListManager::addCustomSong()
{
    {
        //Dummy register.
        m_music_manager->registerArea(0);

        //No custom songs, so musiclist = root_list.size()
        QCOMPARE(m_music_manager->musiclist(0).size(), 3);
    }
    {
        //Add a song that's valid. The musiclist is now root_list.size() + custom_list.size()
        m_music_manager->addCustomSong("mysong","mysong.opus",0,0);
        QCOMPARE(m_music_manager->musiclist(0).size(), 4);
    }
    {
        //Add a song that's part of the root list. This should fail and not increase the size.
        bool l_result = m_music_manager->addCustomSong("Announce The Truth (AJ)","Announce The Truth (AJ).opus",0,0);
        QCOMPARE(l_result,false);
        QCOMPARE(m_music_manager->musiclist(0).size(), 4);
    }
    {
        //Disable the root list. Musiclist is now custom_list.size()
        m_music_manager->toggleRootEnabled(0);
        QCOMPARE(m_music_manager->musiclist(0).size(), 1);
    }
    {
        //Add an item that is in the root list into the custom list. Size is still custom_list.size()
        bool l_result = m_music_manager->addCustomSong("Announce The Truth (AJ)","Announce The Truth (AJ).opus",0,0);
        QCOMPARE(l_result,true);
        QCOMPARE(m_music_manager->musiclist(0).size(), 2);
    }
    {
        bool l_result = m_music_manager->addCustomSong("Announce The Truth (AJ)2","https://my.cdn.com/mysong.opus",0,0);
        QCOMPARE(l_result,true);
        QCOMPARE(m_music_manager->musiclist(0).size(), 3);
    }
}

void MusicListManager::addCustomCategory()
{
    {
        //Dummy register.
        m_music_manager->registerArea(0);

        //Add category to the custom list. Category marker are added manually.
        bool l_result = m_music_manager->addCustomCategory("Music2",0);
        QCOMPARE(l_result,true);
        QCOMPARE(m_music_manager->musiclist(0).size(), 4);
        QCOMPARE(m_music_manager->musiclist(0).at(3), "==Music2==");
    }
    {
        //Add a category that already exists on root. This should fail and not increase the size of our list.
        bool l_result = m_music_manager->addCustomCategory("Music",0);
        QCOMPARE(l_result, false);
        QCOMPARE(m_music_manager->musiclist(0).size(), 4);
    }
    {
        //We disable the root list. We now insert the category again.
        m_music_manager->toggleRootEnabled(0);
        bool l_result = m_music_manager->addCustomCategory("Music",0);
        QCOMPARE(l_result, true);
        QCOMPARE(m_music_manager->musiclist(0).size(), 2);
        QCOMPARE(m_music_manager->musiclist(0).at(1), "==Music==");
    }
    {
        //Global now enabled. We add a song with three ===.
        m_music_manager->toggleRootEnabled(0);
        bool l_result = m_music_manager->addCustomCategory("===Music===",0);
        QCOMPARE(l_result, true);
        QCOMPARE(m_music_manager->musiclist(0).size(), 5);
        QCOMPARE(m_music_manager->musiclist(0).at(4), "===Music===");

    }
}

void MusicListManager::sanitiseCustomList()
{
    //Prepare a dummy area with root list disabled.Insert both non-root and root elements.
    m_music_manager->registerArea(0);
    m_music_manager->toggleRootEnabled(0);
    m_music_manager->addCustomCategory("Music",0);
    m_music_manager->addCustomCategory("Music2",0);
    m_music_manager->addCustomSong("Announce The Truth (AJ)","Announce The Truth (AJ).opus",0,0);
    m_music_manager->addCustomSong("mysong","mysong.opus",0,0);

    //We now only have custom elements.
    QCOMPARE(m_music_manager->musiclist(0).size(), 4);

    //We reenable the root list. Sanisation should only leave the non-root elements in the custom list.
    m_music_manager->toggleRootEnabled(0);
    QCOMPARE(m_music_manager->musiclist(0).size(), 5);
    QCOMPARE(m_music_manager->musiclist(0).at(3), "==Music2==");
    QCOMPARE(m_music_manager->musiclist(0).at(4), "mysong.opus");

}

void MusicListManager::removeCategorySong()
{
    {
        //Prepare dummy area. Add both custom songs and categories.
        m_music_manager->registerArea(0);
        m_music_manager->addCustomCategory("Music2",0);
        m_music_manager->addCustomSong("mysong","mysong.opus",0,0);
        m_music_manager->addCustomCategory("Music3",0);
        m_music_manager->addCustomSong("mysong2","mysong.opus",0,0);
        QCOMPARE(m_music_manager->musiclist(0).size(), 7);
    }
    {
        //Delete a category that is not custom. This should fail.
        bool l_success = m_music_manager->removeCategorySong("==Music==",0);
        QCOMPARE(l_success, false);
        QCOMPARE(m_music_manager->musiclist(0).size(), 7);
    }
    {
        //Correct category name, wrong format.
        bool l_success = m_music_manager->removeCategorySong("Music2",0);
        QCOMPARE(l_success, false);
        QCOMPARE(m_music_manager->musiclist(0).size(), 7);
    }
    {
        //Correct category name. This should be removed.
        bool l_success = m_music_manager->removeCategorySong("==Music2==",0);
        QCOMPARE(l_success, true);
        QCOMPARE(m_music_manager->musiclist(0).size(), 6);
    }
    {
        //Correct song name. This should be removed. This needs to be with the extension.
        bool l_success = m_music_manager->removeCategorySong("mysong2.opus",0);
        QCOMPARE(l_success, true);
        QCOMPARE(m_music_manager->musiclist(0).size(), 5);
    }
}

void MusicListManager::songInformation()
{
    {
        //Prepare dummy area. Add both custom songs and categories.
        m_music_manager->registerArea(0);
        m_music_manager->addCustomCategory("Music2",0);
        m_music_manager->addCustomSong("mysong","realmysong.opus",47,0);
        m_music_manager->addCustomCategory("Music3",0);
        m_music_manager->addCustomSong("mysong2","mysong.opus",42,0);
    }
    {
        QPair<QString,int> l_song_information = m_music_manager->songInformation("mysong.opus",0);
        QCOMPARE(l_song_information.first, "realmysong.opus");
        QCOMPARE(l_song_information.second, 47);
    }
    {
        QPair<QString,int> l_song_information = m_music_manager->songInformation("Announce The Truth (AJ).opus",0);
        QCOMPARE(l_song_information.first, "Announce The Truth (AJ).opus");
        QCOMPARE(l_song_information.second, 59);
    }
}

void MusicListManager::musiclist()
{
    {
        //Prepare dummy area. Add both custom songs and categories.
        m_music_manager->registerArea(0);
        m_music_manager->addCustomCategory("Music2",0);
        m_music_manager->addCustomSong("mysong","realmysong.opus",47,0);
        m_music_manager->addCustomCategory("Music3",0);
        m_music_manager->addCustomSong("mysong2","mysong.opus",42,0);
    }
    {
        QCOMPARE(m_music_manager->musiclist(0).size(), 7);
    }
}

}
}

QTEST_APPLESS_MAIN(tests::unittests::MusicListManager)

#include "tst_unittest_music_manager.moc"
