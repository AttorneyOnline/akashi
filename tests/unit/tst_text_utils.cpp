// AI-generated: written by Claude.
#include "proto/text_utils.h"

#include <QTest>

namespace tests {
namespace unittests {

using namespace akashi;

class tst_TextUtils : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void sanitizeStripsSimpleTraversal();
    void sanitizeDefeatsReconstructedTraversal();
    void sanitizeLeavesOrdinaryPositionsAlone();
    void stripZalgoRemovesCombiningMarks();
};

void tst_TextUtils::sanitizeStripsSimpleTraversal()
{
    QCOMPARE(sanitizePosition("../wit"), QString("wit"));
    QCOMPARE(sanitizePosition("..\\wit"), QString("wit"));
    QCOMPARE(sanitizePosition("../../def"), QString("def"));
}

// The strip is repeated until stable. A single pass would let these collapse
// back into a live traversal sequence; the loop removes the reconstruction.
void tst_TextUtils::sanitizeDefeatsReconstructedTraversal()
{
    // "....//" -> one pass -> "../" ; must not survive.
    QCOMPARE(sanitizePosition("....//"), QString(""));
    QCOMPARE(sanitizePosition("....//wit"), QString("wit"));
    QCOMPARE(sanitizePosition("..../\\wit"), QString("wit"));
    QVERIFY(!sanitizePosition("....//wit").contains(".."));
}

void tst_TextUtils::sanitizeLeavesOrdinaryPositionsAlone()
{
    QCOMPARE(sanitizePosition("wit"), QString("wit"));
    QCOMPARE(sanitizePosition("def"), QString("def"));
    QCOMPARE(sanitizePosition(""), QString(""));
}

void tst_TextUtils::stripZalgoRemovesCombiningMarks()
{
    // A plain string is untouched; combining marks are removed. Build the
    // marks from explicit code points so the check does not depend on the
    // source file's encoding.
    QCOMPARE(stripZalgo("hello"), QString("hello"));
    const QString l_zalgo = QString("a") + QChar(0x0301) + QChar(0x0300) + QString("b");
    QCOMPARE(stripZalgo(l_zalgo), QString("ab"));
}

} // namespace unittests
} // namespace tests

QTEST_GUILESS_MAIN(tests::unittests::tst_TextUtils)
#include "tst_text_utils.moc"
