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
    m_area->clientJoinedArea(5);

    // There must be exactly one client in the area, and it must have a charid of 5.
    QCOMPARE(m_area->charactersTaken().size(), 1);
    QCOMPARE(m_area->charactersTaken().at(0), 5);

    m_area->clientLeftArea(5);

    // No clients must be left in the area.
    QCOMPARE(m_area->charactersTaken().size(), 0);
}

}
}

QTEST_APPLESS_MAIN(tests::unittests::Area)

#include "tst_unittest_area.moc"
