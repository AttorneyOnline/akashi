#include "include/akashiutils.h"
#include <QTest>

namespace tests {
namespace unittests {

class tst_AkashiUtils : public QObject
{

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
