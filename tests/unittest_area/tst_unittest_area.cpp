#include <QtTest>

// add necessary includes here

class UnitTest_Area : public QObject
{
    Q_OBJECT

public:

private slots:
    void test_case1();
};

void UnitTest_Area::test_case1()
{
    QFAIL("Guaranteed failure -- testing tests subdirs setup");
}

QTEST_APPLESS_MAIN(UnitTest_Area)

#include "tst_unittest_area.moc"
