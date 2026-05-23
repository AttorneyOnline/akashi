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
    void ownerTrackedUnregister();
    void hasFilterQuery();
    void gimpReplacesEntireMessage();
    void medievalTransformsText();
    void shakePreservesWordSet();
    void disemvowelStripsVowels();
    void wordFilterRedactsMatches();
};

void tst_TextFilter::singleFilterTransformsText()
{
    TextFilterRegistry l_registry;
    l_registry.registerFilter("upper", 100,
        [](const QString &t) -> std::optional<QString> { return t.toUpper(); },
        false, "test");

    auto l_result = l_registry.apply("hello", {"upper"});
    QVERIFY(l_result.has_value());
    QCOMPARE(*l_result, QString("HELLO"));
}

void tst_TextFilter::multipleFiltersApplyInOrder()
{
    TextFilterRegistry l_registry;
    l_registry.registerFilter("append-b", 200,
        [](const QString &t) -> std::optional<QString> { return t + "B"; },
        false, "test");
    l_registry.registerFilter("append-a", 100,
        [](const QString &t) -> std::optional<QString> { return t + "A"; },
        false, "test");

    auto l_result = l_registry.apply("X", {"append-a", "append-b"});
    QVERIFY(l_result.has_value());
    QCOMPARE(*l_result, QString("XAB"));
}

void tst_TextFilter::insertionOrderTiesbreakEqualOrder()
{
    TextFilterRegistry l_registry;
    l_registry.registerFilter("first", 100,
        [](const QString &t) -> std::optional<QString> { return t + "1"; },
        false, "test");
    l_registry.registerFilter("second", 100,
        [](const QString &t) -> std::optional<QString> { return t + "2"; },
        false, "test");

    auto l_result = l_registry.apply("X", {"first", "second"});
    QVERIFY(l_result.has_value());
    QCOMPARE(*l_result, QString("X12"));
}

void tst_TextFilter::alwaysActiveRunsWithEmptyActiveSet()
{
    TextFilterRegistry l_registry;
    l_registry.registerFilter("censor", 100,
        [](const QString &t) -> std::optional<QString> { return t.toUpper(); },
        true, "test");

    auto l_result = l_registry.apply("hello", {});
    QVERIFY(l_result.has_value());
    QCOMPARE(*l_result, QString("HELLO"));
}

void tst_TextFilter::nonActiveFilterIsSkipped()
{
    TextFilterRegistry l_registry;
    l_registry.registerFilter("upper", 100,
        [](const QString &t) -> std::optional<QString> { return t.toUpper(); },
        false, "test");

    auto l_result = l_registry.apply("hello", {});
    QVERIFY(l_result.has_value());
    QCOMPARE(*l_result, QString("hello"));
}

void tst_TextFilter::nulloptStopsTheChain()
{
    TextFilterRegistry l_registry;
    l_registry.registerFilter("drop", 100,
        [](const QString &) -> std::optional<QString> { return std::nullopt; },
        true, "test");
    l_registry.registerFilter("unreachable", 200,
        [](const QString &t) -> std::optional<QString> { return t + "NOPE"; },
        true, "test");

    auto l_result = l_registry.apply("hello", {});
    QVERIFY(!l_result.has_value());
}

void tst_TextFilter::ownerTrackedUnregister()
{
    TextFilterRegistry l_registry;
    l_registry.registerFilter("core-filter", 100,
        [](const QString &t) -> std::optional<QString> { return t + "C"; },
        true, "core");
    l_registry.registerFilter("plugin-filter", 200,
        [](const QString &t) -> std::optional<QString> { return t + "P"; },
        true, "plugin");

    l_registry.unregisterAll("plugin");

    QVERIFY(l_registry.hasFilter("core-filter"));
    QVERIFY(!l_registry.hasFilter("plugin-filter"));

    auto l_result = l_registry.apply("X", {});
    QCOMPARE(*l_result, QString("XC"));
}

void tst_TextFilter::hasFilterQuery()
{
    TextFilterRegistry l_registry;
    QVERIFY(!l_registry.hasFilter("missing"));

    l_registry.registerFilter("present", 100,
        [](const QString &t) -> std::optional<QString> { return t; },
        false, "test");
    QVERIFY(l_registry.hasFilter("present"));
}

void tst_TextFilter::gimpReplacesEntireMessage()
{
    TextFilterRegistry l_registry;
    l_registry.registerFilter("gimped", 200,
        [](const QString &) -> std::optional<QString> { return QStringLiteral("I am a heinous criminal."); },
        false, "test");

    auto l_result = l_registry.apply("this text is ignored", {"gimped"});
    QCOMPARE(*l_result, QString("I am a heinous criminal."));
}

void tst_TextFilter::medievalTransformsText()
{
    TextFilterRegistry l_registry;
    l_registry.registerFilter("medieval", 300,
        [](const QString &t) -> std::optional<QString> { return "Ye olde " + t; },
        false, "test");

    auto l_result = l_registry.apply("hello", {"medieval"});
    QCOMPARE(*l_result, QString("Ye olde hello"));
}

void tst_TextFilter::shakePreservesWordSet()
{
    TextFilterRegistry l_registry;
    l_registry.registerFilter("shaken", 400,
        [](const QString &t) -> std::optional<QString> {
            QStringList l_words = t.split(" ");
            std::shuffle(l_words.begin(), l_words.end(), *QRandomGenerator::global());
            return l_words.join(" ");
        }, false, "test");

    auto l_result = l_registry.apply("a b c d e", {"shaken"});
    QVERIFY(l_result.has_value());
    QStringList l_words = l_result->split(" ");
    std::sort(l_words.begin(), l_words.end());
    QCOMPARE(l_words, QStringList({"a", "b", "c", "d", "e"}));
}

void tst_TextFilter::disemvowelStripsVowels()
{
    TextFilterRegistry l_registry;
    l_registry.registerFilter("disemvoweled", 500,
        [](const QString &t) -> std::optional<QString> {
            return QString(t).remove(QRegularExpression("[AEIOUaeiou]"));
        }, false, "test");

    auto l_result = l_registry.apply("Hello World", {"disemvoweled"});
    QCOMPARE(*l_result, QString("Hll Wrld"));
}

void tst_TextFilter::wordFilterRedactsMatches()
{
    TextFilterRegistry l_registry;
    l_registry.registerFilter("word-filter", 100,
        [](const QString &t) -> std::optional<QString> {
            QString l_result = t;
            l_result.replace(QRegularExpression("bad", QRegularExpression::CaseInsensitiveOption),
                             QStringLiteral("❌"));
            return l_result;
        }, true, "test");

    auto l_result = l_registry.apply("this is Bad word", {});
    QCOMPARE(*l_result, QString("this is ❌ word"));
}

} // namespace unittests
} // namespace tests

QTEST_GUILESS_MAIN(tests::unittests::tst_TextFilter)
#include "tst_text_filter.moc"
