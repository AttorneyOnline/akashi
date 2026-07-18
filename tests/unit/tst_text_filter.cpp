// AI-generated: written by Claude.
#include "core/text_filter_registry.h"

#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTest>

namespace tests {
namespace unittests {

using namespace akashi;

class tst_TextFilter : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void singleFilterTransformsText();
    void multipleFiltersApplyInOrder();
    void insertionOrderTiesbreakEqualOrder();
    void alwaysActiveRunsWithEmptyActiveSet();
    void nonActiveFilterIsSkipped();
    void nulloptStopsTheChain();
    void dropMidChainNeverRunsLaterFilters();
    void unknownActiveIdIsIgnored();
    void duplicateIdIsRefused();
    void ownerTrackedUnregister();
    void unregisterUnknownOwnerIsANoOp();
    void hasFilterQuery();
    void oocOnlyFilterNeverRunsOnIc();
    void gimpReplacesEntireMessage();
    void medievalTransformsText();
    void shakePreservesWordSet();
    void disemvowelStripsVowels();
    void wordFilterRedactsMatches();
    void icOnlyFilterSkipsTheOocChannel();
    void bothChannelFilterRunsOnBoth();
    void applyFilterRunsOneFilterAlone();
    void filterRegisteringMidApplyDoesNotCorruptTheList();
};

void tst_TextFilter::singleFilterTransformsText()
{
    TextFilterRegistry l_registry;
    l_registry.registerFilter(
        "upper", 100, [](const QString &t) -> std::optional<QString> { return t.toUpper(); }, false, "test");

    auto l_result = l_registry.apply("hello", {"upper"});
    QVERIFY(l_result.has_value());
    QCOMPARE(*l_result, QString("HELLO"));
}

void tst_TextFilter::multipleFiltersApplyInOrder()
{
    TextFilterRegistry l_registry;
    l_registry.registerFilter(
        "append-b", 200, [](const QString &t) -> std::optional<QString> { return t + "B"; }, false, "test");
    l_registry.registerFilter(
        "append-a", 100, [](const QString &t) -> std::optional<QString> { return t + "A"; }, false, "test");

    auto l_result = l_registry.apply("X", {"append-a", "append-b"});
    QVERIFY(l_result.has_value());
    QCOMPARE(*l_result, QString("XAB"));
}

void tst_TextFilter::insertionOrderTiesbreakEqualOrder()
{
    TextFilterRegistry l_registry;
    l_registry.registerFilter(
        "first", 100, [](const QString &t) -> std::optional<QString> { return t + "1"; }, false, "test");
    l_registry.registerFilter(
        "second", 100, [](const QString &t) -> std::optional<QString> { return t + "2"; }, false, "test");

    auto l_result = l_registry.apply("X", {"first", "second"});
    QVERIFY(l_result.has_value());
    QCOMPARE(*l_result, QString("X12"));
}

void tst_TextFilter::alwaysActiveRunsWithEmptyActiveSet()
{
    TextFilterRegistry l_registry;
    l_registry.registerFilter(
        "censor", 100, [](const QString &t) -> std::optional<QString> { return t.toUpper(); }, true, "test");

    auto l_result = l_registry.apply("hello", {});
    QVERIFY(l_result.has_value());
    QCOMPARE(*l_result, QString("HELLO"));
}

void tst_TextFilter::nonActiveFilterIsSkipped()
{
    TextFilterRegistry l_registry;
    l_registry.registerFilter(
        "upper", 100, [](const QString &t) -> std::optional<QString> { return t.toUpper(); }, false, "test");

    auto l_result = l_registry.apply("hello", {});
    QVERIFY(l_result.has_value());
    QCOMPARE(*l_result, QString("hello"));
}

void tst_TextFilter::nulloptStopsTheChain()
{
    TextFilterRegistry l_registry;
    l_registry.registerFilter(
        "drop", 100, [](const QString &) -> std::optional<QString> { return std::nullopt; }, true, "test");
    l_registry.registerFilter(
        "unreachable", 200, [](const QString &t) -> std::optional<QString> { return t + "NOPE"; }, true, "test");

    auto l_result = l_registry.apply("hello", {});
    QVERIFY(!l_result.has_value());
}

void tst_TextFilter::dropMidChainNeverRunsLaterFilters()
{
    TextFilterRegistry l_registry;
    int l_later_runs = 0;
    l_registry.registerFilter(
        "prefix", 100, [](const QString &t) -> std::optional<QString> { return "A" + t; }, true, "test");
    l_registry.registerFilter(
        "drop", 200, [](const QString &) -> std::optional<QString> { return std::nullopt; }, true, "test");
    l_registry.registerFilter(
        "later", 300, [&l_later_runs](const QString &t) -> std::optional<QString> {
            l_later_runs++;
            return t; }, true, "test");

    auto l_result = l_registry.apply("hello", {});
    QVERIFY(!l_result.has_value());
    QCOMPARE(l_later_runs, 0);
}

void tst_TextFilter::unknownActiveIdIsIgnored()
{
    // An active set naming no registered filter changes nothing.
    TextFilterRegistry l_registry;
    l_registry.registerFilter(
        "upper", 100, [](const QString &t) -> std::optional<QString> { return t.toUpper(); }, false, "test");

    auto l_result = l_registry.apply("hello", {"ghost"});
    QVERIFY(l_result.has_value());
    QCOMPARE(*l_result, QString("hello"));
}

void tst_TextFilter::duplicateIdIsRefused()
{
    // A second registration under a known id is refused with a warning
    // naming the id and both owners; only the first ever runs.
    TextFilterRegistry l_registry;
    l_registry.registerFilter(
        "twice", 100, [](const QString &t) -> std::optional<QString> { return t + "A"; }, true, "first");
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("twice.*second.*first"));
    l_registry.registerFilter(
        "twice", 200, [](const QString &t) -> std::optional<QString> { return t + "B"; }, true, "second");

    QCOMPARE(*l_registry.apply("X", {}), QString("XA"));

    // applyFilter reaches the surviving first registration.
    QCOMPARE(*l_registry.applyFilter("twice", "X"), QString("XA"));

    // The refused owner holds nothing, so its sweep removes nothing.
    l_registry.unregisterAll("second");
    QVERIFY(l_registry.hasFilter("twice"));
    QCOMPARE(*l_registry.apply("X", {}), QString("XA"));
}

void tst_TextFilter::ownerTrackedUnregister()
{
    TextFilterRegistry l_registry;
    l_registry.registerFilter(
        "core-filter", 100, [](const QString &t) -> std::optional<QString> { return t + "C"; }, true, "core");
    l_registry.registerFilter(
        "plugin-filter", 200, [](const QString &t) -> std::optional<QString> { return t + "P"; }, true, "plugin");

    l_registry.unregisterAll("plugin");

    QVERIFY(l_registry.hasFilter("core-filter"));
    QVERIFY(!l_registry.hasFilter("plugin-filter"));

    auto l_result = l_registry.apply("X", {});
    QCOMPARE(*l_result, QString("XC"));
}

void tst_TextFilter::unregisterUnknownOwnerIsANoOp()
{
    TextFilterRegistry l_registry;
    l_registry.registerFilter(
        "keeper", 100, [](const QString &t) -> std::optional<QString> { return t + "K"; }, true, "plugin");

    l_registry.unregisterAll("nobody");

    QVERIFY(l_registry.hasFilter("keeper"));
    QCOMPARE(*l_registry.apply("X", {}), QString("XK"));
}

void tst_TextFilter::hasFilterQuery()
{
    TextFilterRegistry l_registry;
    QVERIFY(!l_registry.hasFilter("missing"));

    l_registry.registerFilter(
        "present", 100, [](const QString &t) -> std::optional<QString> { return t; }, false, "test");
    QVERIFY(l_registry.hasFilter("present"));
}

void tst_TextFilter::oocOnlyFilterNeverRunsOnIc()
{
    TextFilterRegistry l_registry;
    l_registry.registerFilter(
        "ooc-notice", 100, [](const QString &t) -> std::optional<QString> { return t.toUpper(); }, true, "test",
        {TextChannel::Ooc});

    QCOMPARE(*l_registry.apply("hello", {}, TextChannel::Ic), QString("hello"));
    QCOMPARE(*l_registry.apply("hello", {}, TextChannel::Ooc), QString("HELLO"));
}

void tst_TextFilter::gimpReplacesEntireMessage()
{
    TextFilterRegistry l_registry;
    l_registry.registerFilter(
        "gimped", 200, [](const QString &) -> std::optional<QString> { return QStringLiteral("I am a heinous criminal."); }, false, "test");

    auto l_result = l_registry.apply("this text is ignored", {"gimped"});
    QCOMPARE(*l_result, QString("I am a heinous criminal."));
}

void tst_TextFilter::medievalTransformsText()
{
    TextFilterRegistry l_registry;
    l_registry.registerFilter(
        "medieval", 300, [](const QString &t) -> std::optional<QString> { return "Ye olde " + t; }, false, "test");

    auto l_result = l_registry.apply("hello", {"medieval"});
    QCOMPARE(*l_result, QString("Ye olde hello"));
}

void tst_TextFilter::shakePreservesWordSet()
{
    TextFilterRegistry l_registry;
    l_registry.registerFilter(
        "shaken", 400, [](const QString &t) -> std::optional<QString> {
            QStringList l_words = t.split(" ");
            std::shuffle(l_words.begin(), l_words.end(), *QRandomGenerator::global());
            return l_words.join(" "); }, false, "test");

    auto l_result = l_registry.apply("a b c d e", {"shaken"});
    QVERIFY(l_result.has_value());
    QStringList l_words = l_result->split(" ");
    std::sort(l_words.begin(), l_words.end());
    QCOMPARE(l_words, QStringList({"a", "b", "c", "d", "e"}));
}

void tst_TextFilter::disemvowelStripsVowels()
{
    TextFilterRegistry l_registry;
    l_registry.registerFilter(
        "disemvoweled", 500, [](const QString &t) -> std::optional<QString> {
            static const QRegularExpression s_vowels("[AEIOUaeiou]");
            return QString(t).remove(s_vowels); }, false, "test");

    auto l_result = l_registry.apply("Hello World", {"disemvoweled"});
    QCOMPARE(*l_result, QString("Hll Wrld"));
}

void tst_TextFilter::wordFilterRedactsMatches()
{
    TextFilterRegistry l_registry;
    l_registry.registerFilter(
        "word-filter", 100, [](const QString &t) -> std::optional<QString> {
            static const QRegularExpression s_bad("bad", QRegularExpression::CaseInsensitiveOption);
            QString l_result = t;
            l_result.replace(s_bad, QStringLiteral("❌"));
            return l_result; }, true, "test");

    auto l_result = l_registry.apply("this is Bad word", {});
    QCOMPARE(*l_result, QString("this is ❌ word"));
}

void tst_TextFilter::icOnlyFilterSkipsTheOocChannel()
{
    // Registration defaults to IC-only, so sanction filters never touch OOC.
    TextFilterRegistry l_registry;
    l_registry.registerFilter(
        "gimped", 200, [](const QString &) -> std::optional<QString> { return QStringLiteral("I am a heinous criminal."); }, true, "test");

    QCOMPARE(*l_registry.apply("hello", {}, TextChannel::Ic), QString("I am a heinous criminal."));
    QCOMPARE(*l_registry.apply("hello", {}, TextChannel::Ooc), QString("hello"));
}

void tst_TextFilter::bothChannelFilterRunsOnBoth()
{
    TextFilterRegistry l_registry;
    l_registry.registerFilter(
        "word-filter", 100, [](const QString &t) -> std::optional<QString> { return QString(t).replace("bad", "❌"); }, true, "test",
        {TextChannel::Ic, TextChannel::Ooc});

    QCOMPARE(*l_registry.apply("a bad word", {}, TextChannel::Ic), QString("a ❌ word"));
    QCOMPARE(*l_registry.apply("a bad word", {}, TextChannel::Ooc), QString("a ❌ word"));
}

void tst_TextFilter::applyFilterRunsOneFilterAlone()
{
    // The apply_filter rule action reaches one named filter directly:
    // channels, active sets and the other filters play no part.
    TextFilterRegistry l_registry;
    l_registry.registerFilter(
        "medieval", 300, [](const QString &t) -> std::optional<QString> { return "Ye olde " + t; }, false, "test");
    l_registry.registerFilter(
        "upper", 400, [](const QString &t) -> std::optional<QString> { return t.toUpper(); }, true, "test");

    QCOMPARE(*l_registry.applyFilter("medieval", "hello"), QString("Ye olde hello"));
    // An unknown id leaves the text unchanged.
    QCOMPARE(*l_registry.applyFilter("missing", "hello"), QString("hello"));
}

// A script filter may register another filter from inside its own callback.
// apply() must iterate a snapshot so that insertion (which can reallocate the
// entry list) does not free the running entry or invalidate the loop. Many
// registrations force the list to grow past its initial capacity; without the
// snapshot this reads freed memory.
void tst_TextFilter::filterRegisteringMidApplyDoesNotCorruptTheList()
{
    TextFilterRegistry l_registry;
    int l_seq = 0;
    l_registry.registerFilter(
        "spawner", 100,
        [&l_registry, &l_seq](const QString &t) -> std::optional<QString> {
            const QString l_id = QStringLiteral("spawned-%1").arg(l_seq++);
            l_registry.registerFilter(
                l_id, 200 + l_seq, [](const QString &s) -> std::optional<QString> { return s; }, true, "test");
            return t + "!";
        },
        true, "test");

    for (int i = 0; i < 64; i++)
        QCOMPARE(*l_registry.apply("x", {}), QString("x!"));

    // Every re-entrant registration landed and the surviving list is intact.
    QVERIFY(l_registry.hasFilter("spawner"));
    QVERIFY(l_registry.hasFilter("spawned-0"));
}

} // namespace unittests
} // namespace tests

QTEST_GUILESS_MAIN(tests::unittests::tst_TextFilter)
#include "tst_text_filter.moc"
