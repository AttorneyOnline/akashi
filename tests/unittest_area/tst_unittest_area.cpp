#include <QtTest>

#include <include/area_data.h>

namespace tests {
namespace unittests {

/**
 * @brief Unit Tester class for the area-related functions.
 */
class Area : public QObject
{
    Q_OBJECT

public:
    AreaData* m_area;

private slots:
    void init();
    void cleanup();

    void clientJoinLeave();

    void areaStatuses_data();
    void areaStatuses();

    void changeCharacter();
};

void Area::init()
{
    m_area = new AreaData("Test Area", 0);
}

void Area::cleanup()
{
    delete m_area;
}

void Area::clientJoinLeave()
{
    {
        // There must be exactly one client in the area, and it must have a charid of 5.
        m_area->clientJoinedArea(5);

        QCOMPARE(m_area->charactersTaken().size(), 1);
        QCOMPARE(m_area->charactersTaken().at(0), 5);
    }
    {
        // No clients must be left in the area.
        m_area->clientLeftArea(5);

        QCOMPARE(m_area->charactersTaken().size(), 0);
    }
}

void Area::areaStatuses_data()
{
    QTest::addColumn<QString>("statusCall");
    QTest::addColumn<AreaData::Status>("expectedStatus");
    QTest::addColumn<bool>("isSuccessful");

    QTest::newRow("Idle") << "idle" << AreaData::Status::IDLE << true;
    QTest::newRow("RP") << "rp" << AreaData::Status::RP << true;
    QTest::newRow("Casing") << "casing" << AreaData::Status::CASING << true;
    QTest::newRow("Looking for players (long)") << "looking-for-players" << AreaData::Status::LOOKING_FOR_PLAYERS << true;
    QTest::newRow("Looking for players (short)") << "lfp" << AreaData::Status::LOOKING_FOR_PLAYERS << true;
    QTest::newRow("Gaming") << "gaming" << AreaData::Status::GAMING << true;
    QTest::newRow("Recess") << "recess" << AreaData::Status::RECESS << true;
    QTest::newRow("Nonsense") << "blah" << AreaData::Status::IDLE << false;
}

void Area::areaStatuses()
{
    QFETCH(QString, statusCall);
    QFETCH(AreaData::Status, expectedStatus);
    QFETCH(bool, isSuccessful);

    bool l_success = m_area->changeStatus(statusCall);

    QCOMPARE(m_area->status(), expectedStatus);
    QCOMPARE(l_success, isSuccessful);
}

void Area::changeCharacter()
{
    {
        // A client with a charid of 6 joins. There's only them in there.
        m_area->clientJoinedArea(6);

        QCOMPARE(m_area->charactersTaken().size(), 1);
        QCOMPARE(m_area->charactersTaken().at(0), 6);
    }
    {
        // Charid 7 is marked as taken. No other client in the area still.
        // Charids 6 and 7 are taken.
        m_area->changeCharacter(-1, 7);

        QCOMPARE(m_area->playerCount(), 1);
        QCOMPARE(m_area->charactersTaken().size(), 2);
        QCOMPARE(m_area->charactersTaken().at(0), 6);
        QCOMPARE(m_area->charactersTaken().at(1), 7);
    }
    {
        // Client switches to charid 8.
        // Charids 8 and 7 are taken.
        m_area->changeCharacter(6, 8);

        QCOMPARE(m_area->playerCount(), 1);
        QCOMPARE(m_area->charactersTaken().size(), 2);
        QCOMPARE(m_area->charactersTaken().at(0), 7);
        QCOMPARE(m_area->charactersTaken().at(1), 8);
    }
    {
        // Charid 7 is unlocked for use.
        // Charid 8 is taken.
        m_area->changeCharacter(7, -1);

        QCOMPARE(m_area->playerCount(), 1);
        QCOMPARE(m_area->charactersTaken().size(), 1);
        QCOMPARE(m_area->charactersTaken().at(0), 8);
    }
}

}
}

QTEST_APPLESS_MAIN(tests::unittests::Area)

#include "tst_unittest_area.moc"
