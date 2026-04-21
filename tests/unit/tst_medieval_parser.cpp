// AI-generated: written by Claude.
#include "medieval_parser.h"

#include <QTest>

namespace tests {
namespace unittests {

// The parser treats its data as valid only when it has at least one prepend
// word, one append word, and one replacement, so every fixture below carries
// them. Prefixing an input with "-" tells degrootify to skip the random
// pre/post words, which keeps single-word outputs deterministic.
static QByteArray dataWith(const QString &f_replacement_entries)
{
    return QString(R"({
        "prepended_words": {"Forsooth, ": "1"},
        "appended_words": {" verily.": "1"},
        "word_replacements": [%1]
    })")
        .arg(f_replacement_entries)
        .toUtf8();
}

// A dataset broad enough to keep every code path live: singular and plural
// matches, a previous-word match, prepends with a count, and a chance gate.
static QByteArray richData()
{
    return QByteArray(R"({
        "prepended_words": {"Forsooth, ": "1", "Hark! ": "1", "Verily, ": "1"},
        "appended_words": {" I say.": "1", " indeed.": "1", " forsooth.": "1"},
        "word_replacements": [
            {"word": ["hello", "hi"], "replacement": ["hail", "well met"], "chance": 1},
            {"word": ["ox"], "word_plural": ["oxen"], "replacement": ["beast"], "replacement_plural": ["beasts"]},
            {"word": ["is"], "prev": ["it"], "replacement": ["aye"]},
            {"word": ["you"], "replacement": ["thou", "thee"], "replacement_prepend": ["good", "kind", "noble"], "prepend_count": 2},
            {"word": ["the"], "replacement": ["ye"], "chance": 2}
        ]
    })");
}

class tst_MedievalParser : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    // Negative: malformed or incomplete data leaves the parser inert.
    void inertData_data();
    void inertData();

    // Negative: malformed replacement entries never crash.
    void malformedEntry_data();
    void malformedEntry();

    // Negative: degenerate inputs never crash or hang.
    void degenerateInput_data();
    void degenerateInput();

    // Positive: the core replacement behaviour.
    void singularReplacement();
    void matchIsCaseInsensitive();
    void firstLetterCasePreserved();
    void multipleWordsEachReplaced();
    void unmatchedWordSurvives();
    void pluralUsesPluralList();
    void pluralFallsBackToSingular();
    void neitherReplacementListLeavesWordAlone();

    // Positive: prepends.
    void prependIsAdded();
    void prependCountPicksThatManyDistinct();
    void prependCountClampedToListSize();
    void zeroPrependCountAddsNone();
    void prependIndexNeverOutOfRange();

    // Positive: previous-word (double word) matching.
    void prevWordMatchJoinsWords();
    void prevWordAbsentDoesNotMatch();

    // The chance gate.
    void chanceOneAlwaysMatches();
    void chanceZeroBehavesAsAlways();
    void highChanceUsuallySkips();

    // Article adjustment and pre/post generation.
    void articleBecomesAnBeforeVowel();
    void dashPrefixSkipsPreAndPost();
    void preAndPostRunWithoutCrashing();

    // A broad fuzz sweep across every path.
    void fuzzNeverCrashes();
};

void tst_MedievalParser::inertData_data()
{
    QTest::addColumn<QByteArray>("data");

    QTest::newRow("not json") << QByteArrayLiteral("this is not json at all");
    QTest::newRow("truncated json") << QByteArray(R"({"prepended_words": {)");
    QTest::newRow("empty bytes") << QByteArray();
    QTest::newRow("empty object") << QByteArrayLiteral("{}");
    QTest::newRow("json array") << QByteArrayLiteral("[1, 2, 3]");
    QTest::newRow("json null") << QByteArrayLiteral("null");
    QTest::newRow("json number") << QByteArrayLiteral("42");
    QTest::newRow("no prepends") << QByteArray(R"({"appended_words":{"x":"1"},"word_replacements":[{"word":["foo"],"replacement":["bar"]}]})");
    QTest::newRow("empty prepends") << QByteArray(R"({"prepended_words":{},"appended_words":{"x":"1"},"word_replacements":[{"word":["foo"],"replacement":["bar"]}]})");
    QTest::newRow("no appends") << QByteArray(R"({"prepended_words":{"x":"1"},"word_replacements":[{"word":["foo"],"replacement":["bar"]}]})");
    QTest::newRow("empty appends") << QByteArray(R"({"prepended_words":{"x":"1"},"appended_words":{},"word_replacements":[{"word":["foo"],"replacement":["bar"]}]})");
    QTest::newRow("no replacements") << QByteArray(R"({"prepended_words":{"x":"1"},"appended_words":{"y":"1"}})");
    QTest::newRow("empty replacements") << QByteArray(R"({"prepended_words":{"x":"1"},"appended_words":{"y":"1"},"word_replacements":[]})");
    QTest::newRow("wrong types for sections") << QByteArray(R"({"prepended_words":"nope","appended_words":5,"word_replacements":"no"})");
}

void tst_MedievalParser::inertData()
{
    QFETCH(QByteArray, data);
    MedievalParser l_parser(data);

    // An inert parser returns every message exactly as it came in - it does
    // not even strip the "-" prefix, since that only happens on the live path.
    QCOMPARE(l_parser.degrootify("hello foo bar"), QString("hello foo bar"));
    QCOMPARE(l_parser.degrootify("-hello foo bar"), QString("-hello foo bar"));
    QCOMPARE(l_parser.degrootify(""), QString(""));
    QCOMPARE(l_parser.degrootify("&&&"), QString("&&&"));
}

void tst_MedievalParser::malformedEntry_data()
{
    QTest::addColumn<QString>("entry");
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("must_contain");

    QTest::newRow("no replacement key") << QString(R"({"word":["foo"]})") << QStringLiteral("-foo") << QStringLiteral("foo");
    QTest::newRow("empty replacement list") << QString(R"({"word":["foo"],"replacement":[]})") << QStringLiteral("-foo") << QStringLiteral("foo");
    QTest::newRow("null replacement") << QString(R"({"word":["foo"],"replacement":null})") << QStringLiteral("-foo") << QStringLiteral("foo");
    QTest::newRow("object replacement") << QString(R"({"word":["foo"],"replacement":{}})") << QStringLiteral("-foo") << QStringLiteral("foo");
    QTest::newRow("string word coerced") << QString(R"({"word":"foo","replacement":["bar"]})") << QStringLiteral("-foo") << QStringLiteral("bar");
    QTest::newRow("string replacement coerced") << QString(R"({"word":["foo"],"replacement":"bar"})") << QStringLiteral("-foo") << QStringLiteral("bar");
    QTest::newRow("number in replacement list") << QString(R"({"word":["foo"],"replacement":[7]})") << QStringLiteral("-foo") << QStringLiteral("7");
    QTest::newRow("non-numeric chance") << QString(R"({"word":["foo"],"replacement":["bar"],"chance":"high"})") << QStringLiteral("-foo") << QStringLiteral("bar");
    QTest::newRow("non-numeric prepend count") << QString(R"({"word":["foo"],"replacement":["bar"],"replacement_prepend":["p"],"prepend_count":"lots"})") << QStringLiteral("-foo") << QStringLiteral("bar");
    QTest::newRow("no word key") << QString(R"({"replacement":["bar"]})") << QStringLiteral("-foo") << QStringLiteral("foo");
    QTest::newRow("empty word list") << QString(R"({"word":[],"replacement":["bar"]})") << QStringLiteral("-foo") << QStringLiteral("foo");
    QTest::newRow("plural fallback to singular") << QString(R"({"word":["ox"],"word_plural":["oxen"],"replacement":["beast"]})") << QStringLiteral("-oxen") << QStringLiteral("beast");
    QTest::newRow("plural with neither list") << QString(R"({"word":["ox"],"word_plural":["oxen"]})") << QStringLiteral("-oxen") << QStringLiteral("oxen");
    QTest::newRow("prepend count exceeds list") << QString(R"({"word":["foo"],"replacement":["bar"],"replacement_prepend":["p"],"prepend_count":9})") << QStringLiteral("-foo") << QStringLiteral("bar");
    QTest::newRow("huge prepend count") << QString(R"({"word":["foo"],"replacement":["bar"],"replacement_prepend":["p"],"prepend_count":2000000})") << QStringLiteral("-foo") << QStringLiteral("bar");
    QTest::newRow("negative prepend count") << QString(R"({"word":["foo"],"replacement":["bar"],"replacement_prepend":["p"],"prepend_count":-3})") << QStringLiteral("-foo") << QStringLiteral("bar");
    QTest::newRow("negative chance") << QString(R"({"word":["foo"],"replacement":["bar"],"chance":-5})") << QStringLiteral("-foo") << QStringLiteral("bar");
}

void tst_MedievalParser::malformedEntry()
{
    QFETCH(QString, entry);
    QFETCH(QString, input);
    QFETCH(QString, must_contain);

    MedievalParser l_parser(dataWith(entry));
    const QString l_out = l_parser.degrootify(input);
    QVERIFY2(l_out.contains(must_contain),
             qPrintable(QStringLiteral("'%1' -> '%2', expected to contain '%3'").arg(input, l_out, must_contain)));
}

void tst_MedievalParser::degenerateInput_data()
{
    QTest::addColumn<QString>("input");

    QTest::newRow("empty") << QString();
    QTest::newRow("single space") << QStringLiteral(" ");
    QTest::newRow("many spaces") << QStringLiteral("      ");
    QTest::newRow("dash only") << QStringLiteral("-");
    QTest::newRow("double dash") << QStringLiteral("--");
    QTest::newRow("dash space") << QStringLiteral("- ");
    QTest::newRow("punctuation") << QStringLiteral("!?.,;:");
    QTest::newRow("ampersands") << QStringLiteral("&&&&");
    QTest::newRow("single ampersand") << QStringLiteral("&");
    QTest::newRow("ampersand word") << QStringLiteral("&godword");
    QTest::newRow("single letter") << QStringLiteral("a");
    QTest::newRow("single upper h") << QStringLiteral("H");
    QTest::newRow("numbers") << QStringLiteral("12345");
    QTest::newRow("mixed symbols") << QStringLiteral("a&b#c%d$e");
    QTest::newRow("apostrophes") << QStringLiteral("''''");
    QTest::newRow("trailing apostrophe") << QStringLiteral("its'");
    QTest::newRow("unicode") << QString::fromUtf8("héllo wörld \xE2\x98\xBA");
    QTest::newRow("newlines") << QStringLiteral("line1\nline2\n");
    QTest::newRow("tabs") << QStringLiteral("a\tb\tc");
    QTest::newRow("long sentence") << QStringLiteral("word ").repeated(500);
    QTest::newRow("long single token") << QStringLiteral("a").repeated(5000);
    QTest::newRow("ing endings") << QStringLiteral("coming going running singing");
    QTest::newRow("ed endings") << QStringLiteral("walked talked jumped");
    QTest::newRow("ke endings") << QStringLiteral("bike like make take");
    QTest::newRow("ss endings") << QStringLiteral("boss loss moss glass");
    QTest::newRow("eth triggers") << QStringLiteral("cat cap sick dog crab law");
    QTest::newRow("h words") << QStringLiteral("hello here how happy");
    QTest::newRow("two letter words") << QStringLiteral("it is at on in");
};

void tst_MedievalParser::degenerateInput()
{
    QFETCH(QString, input);

    // A rich dataset so the random transform paths are all reachable. The only
    // requirement is that no input crashes or hangs; a per-test timeout guards
    // against a hang. Run each input many times to exercise the random paths.
    MedievalParser l_parser(richData());
    for (int i = 0; i < 25; i++) {
        l_parser.degrootify(input);
        l_parser.degrootify(QStringLiteral("-") + input);
    }
    QVERIFY(true); // reached only if nothing above crashed
}

void tst_MedievalParser::singularReplacement()
{
    MedievalParser l_parser(dataWith(QString(R"({"word":["hello"],"replacement":["hail"]})")));
    QCOMPARE(l_parser.degrootify(QStringLiteral("-hello")), QString("hail"));
}

void tst_MedievalParser::matchIsCaseInsensitive()
{
    MedievalParser l_parser(dataWith(QString(R"({"word":["hello"],"replacement":["hail"]})")));
    QVERIFY(l_parser.degrootify(QStringLiteral("-HELLO")).contains(QStringLiteral("ail")));
    QVERIFY(l_parser.degrootify(QStringLiteral("-Hello")).contains(QStringLiteral("ail")));
}

void tst_MedievalParser::firstLetterCasePreserved()
{
    MedievalParser l_parser(dataWith(QString(R"({"word":["hello"],"replacement":["hail"]})")));
    QCOMPARE(l_parser.degrootify(QStringLiteral("-Hello")), QString("Hail"));
    QCOMPARE(l_parser.degrootify(QStringLiteral("-hello")), QString("hail"));
}

void tst_MedievalParser::multipleWordsEachReplaced()
{
    MedievalParser l_parser(dataWith(QString(R"({"word":["hello"],"replacement":["hail"]},{"word":["world"],"replacement":["realm"]})")));
    const QString l_out = l_parser.degrootify(QStringLiteral("-hello world"));
    QVERIFY(l_out.contains(QStringLiteral("hail")));
    QVERIFY(l_out.contains(QStringLiteral("realm")));
}

void tst_MedievalParser::unmatchedWordSurvives()
{
    MedievalParser l_parser(dataWith(QString(R"({"word":["zzz"],"replacement":["yyy"]})")));
    // "it" matches no entry and no character rule, so it comes through as-is.
    QCOMPARE(l_parser.degrootify(QStringLiteral("-it")), QString("it"));
}

void tst_MedievalParser::pluralUsesPluralList()
{
    MedievalParser l_parser(dataWith(QString(R"({"word":["ox"],"word_plural":["oxen"],"replacement":["beast"],"replacement_plural":["herd"]})")));
    QCOMPARE(l_parser.degrootify(QStringLiteral("-oxen")), QString("herd"));
    QCOMPARE(l_parser.degrootify(QStringLiteral("-ox")), QString("beast"));
}

void tst_MedievalParser::pluralFallsBackToSingular()
{
    MedievalParser l_parser(dataWith(QString(R"({"word":["ox"],"word_plural":["oxen"],"replacement":["beast"]})")));
    QCOMPARE(l_parser.degrootify(QStringLiteral("-oxen")), QString("beast"));
}

void tst_MedievalParser::neitherReplacementListLeavesWordAlone()
{
    MedievalParser l_parser(dataWith(QString(R"({"word":["ox"],"word_plural":["oxen"]})")));
    QCOMPARE(l_parser.degrootify(QStringLiteral("-oxen")), QString("oxen"));
    QCOMPARE(l_parser.degrootify(QStringLiteral("-ox")), QString("ox"));
}

void tst_MedievalParser::prependIsAdded()
{
    MedievalParser l_parser(dataWith(QString(R"({"word":["foo"],"replacement":["bar"],"replacement_prepend":["good"],"prepend_count":1})")));
    QCOMPARE(l_parser.degrootify(QStringLiteral("-foo")), QString("good bar"));
}

void tst_MedievalParser::prependCountPicksThatManyDistinct()
{
    MedievalParser l_parser(dataWith(QString(R"({"word":["foo"],"replacement":["bar"],"replacement_prepend":["aaa","bbb","ccc"],"prepend_count":2})")));
    for (int i = 0; i < 100; i++) {
        const QString l_out = l_parser.degrootify(QStringLiteral("-foo"));
        const int l_picked = l_out.count(QStringLiteral("aaa")) + l_out.count(QStringLiteral("bbb")) + l_out.count(QStringLiteral("ccc"));
        QCOMPARE(l_picked, 2); // exactly two distinct prepends, never a repeat
        QVERIFY(l_out.contains(QStringLiteral("bar")));
    }
}

void tst_MedievalParser::prependCountClampedToListSize()
{
    // prepend_count is larger than the list; it must clamp instead of looping
    // forever looking for a third distinct index.
    MedievalParser l_parser(dataWith(QString(R"({"word":["foo"],"replacement":["bar"],"replacement_prepend":["aaa","bbb"],"prepend_count":5})")));
    const QString l_out = l_parser.degrootify(QStringLiteral("-foo"));
    QVERIFY(l_out.contains(QStringLiteral("aaa")));
    QVERIFY(l_out.contains(QStringLiteral("bbb")));
    QVERIFY(l_out.contains(QStringLiteral("bar")));
}

void tst_MedievalParser::zeroPrependCountAddsNone()
{
    MedievalParser l_parser(dataWith(QString(R"({"word":["foo"],"replacement":["bar"],"replacement_prepend":["nope"],"prepend_count":0})")));
    QCOMPARE(l_parser.degrootify(QStringLiteral("-foo")), QString("bar"));
}

void tst_MedievalParser::prependIndexNeverOutOfRange()
{
    // A single prepend with count 1: the old code drew an index up to and
    // including the list size, one past the end. Hammer it.
    MedievalParser l_parser(dataWith(QString(R"({"word":["foo"],"replacement":["bar"],"replacement_prepend":["solo"],"prepend_count":1})")));
    for (int i = 0; i < 500; i++) {
        QCOMPARE(l_parser.degrootify(QStringLiteral("-foo")), QString("solo bar"));
    }
}

void tst_MedievalParser::prevWordMatchJoinsWords()
{
    // "is" only matches when the previous word is "it"; the two collapse into
    // the replacement. This also proves prev-words reach word_vector.
    MedievalParser l_parser(dataWith(QString(R"({"word":["is"],"prev":["it"],"replacement":["aye"]})")));
    const QString l_out = l_parser.degrootify(QStringLiteral("-it is"));
    QVERIFY(l_out.contains(QStringLiteral("aye")));
    QVERIFY(!l_out.contains(QStringLiteral("it is")));
}

void tst_MedievalParser::prevWordAbsentDoesNotMatch()
{
    MedievalParser l_parser(dataWith(QString(R"({"word":["is"],"prev":["it"],"replacement":["aye"]})")));
    const QString l_out = l_parser.degrootify(QStringLiteral("-she is"));
    QVERIFY(!l_out.contains(QStringLiteral("aye")));
    QVERIFY(l_out.contains(QStringLiteral("is")));
}

void tst_MedievalParser::chanceOneAlwaysMatches()
{
    MedievalParser l_parser(dataWith(QString(R"({"word":["foo"],"replacement":["bar"],"chance":1})")));
    for (int i = 0; i < 50; i++) {
        QCOMPARE(l_parser.degrootify(QStringLiteral("-foo")), QString("bar"));
    }
}

void tst_MedievalParser::chanceZeroBehavesAsAlways()
{
    // chance 0 exercises randomInt(1, 0); it must not throw and behaves like
    // an always-on gate rather than corrupting the match.
    MedievalParser l_parser(dataWith(QString(R"({"word":["foo"],"replacement":["bar"],"chance":0})")));
    for (int i = 0; i < 50; i++) {
        QCOMPARE(l_parser.degrootify(QStringLiteral("-foo")), QString("bar"));
    }
}

void tst_MedievalParser::highChanceUsuallySkips()
{
    MedievalParser l_parser(dataWith(QString(R"({"word":["foo"],"replacement":["bar"],"chance":1000000})")));
    int l_unchanged = 0;
    for (int i = 0; i < 100; i++) {
        if (l_parser.degrootify(QStringLiteral("-foo")) == QStringLiteral("foo")) {
            l_unchanged++;
        }
    }
    // With a one-in-a-million chance, essentially every attempt should skip.
    QVERIFY2(l_unchanged >= 90, qPrintable(QStringLiteral("only %1/100 skipped").arg(l_unchanged)));
}

void tst_MedievalParser::articleBecomesAnBeforeVowel()
{
    MedievalParser l_parser(dataWith(QString(R"({"word":["fruit"],"replacement":["apple"]})")));
    QVERIFY(l_parser.degrootify(QStringLiteral("-a fruit")).contains(QStringLiteral("an apple")));
}

void tst_MedievalParser::dashPrefixSkipsPreAndPost()
{
    MedievalParser l_parser(dataWith(QString(R"({"word":["foo"],"replacement":["bar"]})")));
    for (int i = 0; i < 100; i++) {
        // No prepended/appended words are ever added when the input starts "-".
        QCOMPARE(l_parser.degrootify(QStringLiteral("-foo")), QString("bar"));
    }
}

void tst_MedievalParser::preAndPostRunWithoutCrashing()
{
    MedievalParser l_parser(dataWith(QString(R"({"word":["foo"],"replacement":["bar"]})")));
    for (int i = 0; i < 200; i++) {
        // Random pre/post words may wrap the result, but the replacement is
        // always present and the pre/post path never crashes.
        QVERIFY(l_parser.degrootify(QStringLiteral("foo")).contains(QStringLiteral("bar")));
    }
}

void tst_MedievalParser::fuzzNeverCrashes()
{
    MedievalParser l_parser(richData());
    const QStringList l_corpus = {
        QString(),
        QStringLiteral(" "),
        QStringLiteral("-"),
        QStringLiteral("hello world"),
        QStringLiteral("it is what it is"),
        QStringLiteral("the oxen you"),
        QStringLiteral("COMING GOING WALKED"),
        QStringLiteral("walked-talked bike'd"),
        QStringLiteral("a&b c&d &godword"),
        QStringLiteral("'apostrophe' test's"),
        QString::fromUtf8("\xC3\xBCn\xC3\xAF\x63\xC3\xB6\x64\xC3\xA9"),
        QStringLiteral("spam ").repeated(200),
        QStringLiteral("hello. you? the! is,"),
        QStringLiteral("&hello &world"),
        QStringLiteral("--double dash"),
        QStringLiteral("H"),
        QStringLiteral("&"),
        QStringLiteral("boss glass moss loss"),
    };
    for (const QString &l_base : l_corpus) {
        for (int i = 0; i < 20; i++) {
            l_parser.degrootify(l_base);
            l_parser.degrootify(QStringLiteral("-") + l_base);
        }
    }
    QVERIFY(true); // every path above returned without crashing or hanging
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_MedievalParser)

#include "tst_medieval_parser.moc"
