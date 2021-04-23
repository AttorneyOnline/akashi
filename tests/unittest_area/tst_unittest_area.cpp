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

private slots:
    /**
     * @test Example test case 1.
     */
    void test_case1();
};

void Area::test_case1()
{
    QFAIL("Guaranteed failure -- testing tests subdirs setup");
}

}
}

QTEST_APPLESS_MAIN(tests::unittests::Area)

#include "tst_unittest_area.moc"
