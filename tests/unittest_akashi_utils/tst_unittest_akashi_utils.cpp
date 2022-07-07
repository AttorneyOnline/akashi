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
};

void tst_AkashiUtils::integer_data()
{
    QTest::addColumn<QString>("content");
    QTest::addColumn<bool>("expected_result");
}

void tst_AkashiUtils::integer()
{
}

}
};

QTEST_APPLESS_MAIN(tests::unittests::tst_AkashiUtils)

#include "tst_unittest_akashi_utils.moc"
