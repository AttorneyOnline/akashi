#include "include/akashiutils.h"
#include <QTest>

namespace tests {
namespace unittests {

class tst_AkashiUtils : public QObject
{
    Q_OBJECT

  private slots:

    void integer_data();
    void integer();

    void floating_data();
    void floating();

    void boolean_data();
    void boolean();

    void doublep_data();
    void doublep();
};

void tst_AkashiUtils::integer_data()
{
    QTest::addColumn<QString>("content");
    QTest::addColumn<bool>("expected_result");

    QTest::addRow("Integer (good)") << "one"
                                    << false;
    QTest::addRow("Integer (bad)") << "1"
                                   << true;
}

void tst_AkashiUtils::integer()
{
    QFETCH(QString, content);
    QFETCH(bool, expected_result);

    bool result = AkashiUtils::checkArgType<int>(content);
    QCOMPARE(result, expected_result);
}

void tst_AkashiUtils::floating_data()
{
    QTest::addColumn<QString>("content");
    QTest::addColumn<bool>("expected_result");

    QTest::addRow("Float (good)") << "test"
                                  << false;
    QTest::addRow("Float (bad)") << "3.141"
                                 << true;
}

void tst_AkashiUtils::floating()
{
    QFETCH(QString, content);
    QFETCH(bool, expected_result);

    bool result = AkashiUtils::checkArgType<float>(content);
    QCOMPARE(result, expected_result);
}

void tst_AkashiUtils::boolean_data()
{
    QTest::addColumn<QString>("content");
    QTest::addColumn<bool>("expected_result");

    QTest::addRow("Boolean (random string)") << "test"
                                             << true;
    QTest::addRow("Boolean (true/false string)") << "true"
                                                 << true;
}

void tst_AkashiUtils::boolean()
{
    QFETCH(QString, content);
    QFETCH(bool, expected_result);

    bool result = AkashiUtils::checkArgType<bool>(content);
    QCOMPARE(result, expected_result);
}

void tst_AkashiUtils::doublep_data()
{
    QTest::addColumn<QString>("content");
    QTest::addColumn<bool>("expected_result");

    QTest::addRow("Double (good)") << "test"
                                   << false;
    QTest::addRow("Double (bad)") << "3.141592653589793"
                                  << true;
}

void tst_AkashiUtils::doublep()
{
    QFETCH(QString, content);
    QFETCH(bool, expected_result);

    bool result = AkashiUtils::checkArgType<double>(content);
    QCOMPARE(result, expected_result);
}

}
};

QTEST_APPLESS_MAIN(tests::unittests::tst_AkashiUtils)

#include "tst_unittest_akashi_utils.moc"
