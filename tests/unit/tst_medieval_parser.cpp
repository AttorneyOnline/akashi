// AI-generated: written by Claude.
#include <QTest>

#include "medieval_parser.h"

namespace tests {
namespace unittests {

// The parser needs a non-empty prepend and append list to consider its data
// valid, so every fixture below carries one. The "-" prefix on an input tells
// degrootify to skip the random pre/post words, keeping the output determinstic.
static QByteArray dataWith(const QString &f_replacements)
{
    return QString(R"({
        "prepended_words": {"Forsooth, ": "1"},
        "appended_words": {" verily.": "1"},
        "word_replacements": [%1]
    })")
        .arg(f_replacements)
        .toUtf8();
}

class tst_MedievalParser : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void invalidDataLeavesTheMessageAlone();
    void basicReplacement();
    void missingChanceStillMatches();
    void emptyReplacementListDoesNotCrash();
    void pluralMatchFallsBackToSingular();
    void tooManyPrependsTerminate();
    void prependSelectionStaysInBounds();
};

void tst_MedievalParser::invalidDataLeavesTheMessageAlone()
{
    // Unparseable data, and valid data with an empty prepend list, both leave
    // the parser inert - the message comes back untouched instead of crashing.
    MedievalParser l_garbage(QByteArrayLiteral("this is not json"));
    QCOMPARE(l_garbage.degrootify("hello there"), QString("hello there"));

    MedievalParser l_no_prepends(QByteArrayLiteral(R"({"prepended_words": {}, "appended_words": {"x": "1"}})"));
    QCOMPARE(l_no_prepends.degrootify("hello there"), QString("hello there"));
}

void tst_MedievalParser::basicReplacement()
{
    MedievalParser l_parser(dataWith(R"({"word": ["hello"], "replacement": ["hail"]})"));
    QVERIFY(l_parser.degrootify("-hello").contains("hail"));
}

void tst_MedievalParser::missingChanceStillMatches()
{
    // With no "chance" key the field must default to 1, not uninitialised
    // garbage, so the word matches every time.
    MedievalParser l_parser(dataWith(R"({"word": ["hello"], "replacement": ["hail"]})"));
    for (int i = 0; i < 20; i++) {
        QVERIFY(l_parser.degrootify("-hello").contains("hail"));
    }
}

void tst_MedievalParser::emptyReplacementListDoesNotCrash()
{
    // A word that matches but has no replacement words used to index an empty
    // list. Now the word is simply left as it was.
    MedievalParser l_parser(dataWith(R"({"word": ["foo"]})"));
    QCOMPARE(l_parser.degrootify("-foo"), QString("foo"));
}

void tst_MedievalParser::pluralMatchFallsBackToSingular()
{
    // A plural match with no plural replacement list falls back to the singular
    // list rather than indexing the empty plural list.
    MedievalParser l_parser(dataWith(R"({"word": ["ox"], "word_plural": ["oxen"], "replacement": ["beast"]})"));
    QVERIFY(l_parser.degrootify("-oxen").contains("beast"));
}

void tst_MedievalParser::tooManyPrependsTerminate()
{
    // prepend_count exceeds the number of available prepends. The old "no
    // repeats" loop could never find enough distinct indices and span forever;
    // now it stops at what is available. A test timeout catches a regression.
    MedievalParser l_parser(dataWith(R"({"word": ["foo"], "replacement": ["bar"], "replacement_prepend": ["baz"], "prepend_count": 5})"));
    const QString l_result = l_parser.degrootify("-foo");
    QVERIFY(l_result.contains("bar"));
    QVERIFY(l_result.contains("baz"));
}

void tst_MedievalParser::prependSelectionStaysInBounds()
{
    // Picking prepends used to draw an index up to and including the list size,
    // one past the end. Hammer the path to be sure it never reads out of range.
    MedievalParser l_parser(dataWith(R"({"word": ["foo"], "replacement": ["bar"], "replacement_prepend": ["p1", "p2"], "prepend_count": 2})"));
    for (int i = 0; i < 200; i++) {
        const QString l_result = l_parser.degrootify("-foo");
        QVERIFY(l_result.contains("bar"));
        QVERIFY(l_result.contains("p1"));
        QVERIFY(l_result.contains("p2"));
    }
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_MedievalParser)

#include "tst_medieval_parser.moc"
