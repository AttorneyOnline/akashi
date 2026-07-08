// AI-generated: written by Claude.
#include "akashi/config_store.h"
#include "world/area.h"

#include <QSettings>
#include <QtTest>

Q_DECLARE_METATYPE(akashi::Area::Side);

namespace tests {
namespace unittests {

class Area : public QObject
{
    Q_OBJECT

  public:
    akashi::Area *m_area;
    akashi::ConfigStore *m_store = nullptr;
    QSettings *m_areas_ini = nullptr;
    QSettings *m_ambience_ini = nullptr;

  private Q_SLOTS:
    void initTestCase();
    void init();

    void cleanup();

    void clientJoinLeave();

    void areaStatuses_data();

    void areaStatuses();

    void changeHP_data();

    void changeHP();

    void changeCharacter();
};

void Area::initTestCase()
{
    m_store = new akashi::ConfigStore("config", this);
    m_areas_ini = m_store->settings("areas");
    m_ambience_ini = m_store->settings("ambience");
}

void Area::init()
{
    m_area = new akashi::Area("Test Area", 0, 0, 0, m_areas_ini, m_ambience_ini);
}

void Area::cleanup()
{
    delete m_area;
}

void Area::clientJoinLeave()
{
    {
        // There must be exactly one client in the area, and it must have a charid of 5 and userid 0.
        m_area->addClient(5, 0);

        QCOMPARE(m_area->players().size(), 1);
        QCOMPARE(m_area->charactersTaken().at(0), 5);
    }
    {
        // No clients must be left in the area.
        m_area->removeClient(5, 0);

        QCOMPARE(m_area->players().size(), 0);
    }
}

void Area::areaStatuses_data()
{
    QTest::addColumn<QString>("statusCall");
    QTest::addColumn<QString>("expectedStatus");
    QTest::addColumn<bool>("isSuccessful");

    QTest::newRow("Idle") << "idle"
                          << "IDLE" << true;
    QTest::newRow("RP") << "rp"
                        << "RP" << true;
    QTest::newRow("Casing") << "casing"
                            << "CASING" << true;
    QTest::newRow("Looking for players (long)") << "looking-for-players"
                                                << "LOOKING-FOR-PLAYERS" << true;
    QTest::newRow("Looking for players (short)") << "lfp"
                                                 << "LOOKING-FOR-PLAYERS" << true;
    QTest::newRow("Gaming") << "gaming"
                            << "GAMING" << true;
    QTest::newRow("Recess") << "recess"
                            << "RECESS" << true;
    QTest::newRow("Nonsense") << "blah"
                              << "IDLE" << false;
}

void Area::areaStatuses()
{
    QFETCH(QString, statusCall);
    QFETCH(QString, expectedStatus);
    QFETCH(bool, isSuccessful);

    bool l_success = m_area->changeStatus(statusCall);

    QCOMPARE(m_area->status(), expectedStatus);
    QCOMPARE(l_success, isSuccessful);
}

void Area::changeHP_data()
{
    QTest::addColumn<akashi::Area::Side>("side");
    QTest::addColumn<int>("setHP");
    QTest::addColumn<int>("expectedHP");

    QTest::newRow("Set = Expected (DEF)") << akashi::Area::Side::DEFENCE << 3 << 3;
    QTest::newRow("Set = Expected (PRO)") << akashi::Area::Side::PROSECUTOR << 5 << 5;
    QTest::newRow("Below Zero (DEF)") << akashi::Area::Side::DEFENCE << -5 << 0;
    QTest::newRow("Below Zero (PRO)") << akashi::Area::Side::PROSECUTOR << -7 << 0;
    QTest::newRow("Above Ten (DEF)") << akashi::Area::Side::DEFENCE << 12 << 10;
    QTest::newRow("Above Ten (PRO)") << akashi::Area::Side::PROSECUTOR << 14 << 10;
}

void Area::changeHP()
{
    QFETCH(akashi::Area::Side, side);
    QFETCH(int, setHP);
    QFETCH(int, expectedHP);

    m_area->changeHP(side, setHP);

    if (akashi::Area::Side::DEFENCE == side) {
        QCOMPARE(expectedHP, m_area->defHP());
    }
    else {
        QCOMPARE(expectedHP, m_area->proHP());
    }
}

void Area::changeCharacter()
{
    {
        // A client with a charid of 6 and userid 0 joins. There's only them in there.
        m_area->addClient(6, 0);

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
        // Client attempts to switch to 7.
        // Nothing changes, as it is already taken.

        m_area->changeCharacter(8, 7);

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

#include "tst_area.moc"
