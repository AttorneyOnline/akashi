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
     * @brief Tests changing whatever, gonna write this later
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


};

void MusicListManager::init()
{
    QMap<QString,QPair<QString,float>> l_test_list;
    l_test_list.insert("==Music==",{"==Music==",0});
    l_test_list.insert("Announce The Truth (AJ).opus",{"Announce The Truth (AJ).opus",59.5});
    l_test_list.insert("Announce The Truth (JFA).opus",{"Announce The Truth (JFA).opus",98.5});

    m_music_manager = new MusicManager(l_test_list);
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
        //We also add a song we know does not exist yet.
        m_music_manager->registerArea(0);
        m_music_manager->addCustomSong("somesong.opus","somesong.opus",0,0);
        QCOMPARE(m_music_manager->toggleRootEnabled(0), false);
        QCOMPARE(m_music_manager->musiclist(0).size(), 1);
    }
    {
        //We toggle it again. It should return true now.
        //Since this is now true, we should have the root list with customs cleared.
        QCOMPARE(m_music_manager->toggleRootEnabled(0), true);
        QCOMPARE(m_music_manager->musiclist(0).size(), 3);
    }
    {
        //We now append a valid custom song. We should now have 4 entries on our musiclist.
        m_music_manager->addCustomSong("somesong.opus","somesong.opus",0,0);
        QCOMPARE(m_music_manager->musiclist(0).size(), 4);
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

}
}

QTEST_APPLESS_MAIN(tests::unittests::MusicListManager)

#include "tst_unittest_music_manager.moc"
