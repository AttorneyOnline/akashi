// AI-generated: written by Claude.
#include "world/testimony_recorder.h"

#include <QTest>

using akashi::Statement;
using akashi::TestimonyRecorder;

namespace tests {
namespace unittests {

// The outgoing IC field layout the recorder stores: 15 base fields, or 23
// with the pair block. Only the fields the tests read get real values.
static QStringList baseFields(const QString &f_message, const QString &f_char_id = "5")
{
    return {"1", "pre", "Phoenix", "normal", f_message, "def", "sfx", "0",
            f_char_id, "0", "0", "0", "0", "0", "0"};
}

class tst_Testimony : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void statementReadsNamedFields();
    void malformedStatementReadsAsEmptyValues();
    void playbackFieldsBelongToNobody();
    void playbackFillsEmptyShownameFromTheFolderName();
    void recordingAdvancesTheIndex();
    void editsIgnorePositionsThatDoNotExist();
    void jumpNeedsAtLeastOneStatement();
    void jumpLoopsPastTheEndAndStaysAtTheFirst();
    void clearForgetsEverything();
    void savedLineRoundTripsThroughTheIcParser();
    void savedLineKeepsThePairBlock();
    void shortSavedLineReadsAsNothing();
    void savedLineClearsTheCharId();
    void savedLineNormalizesGarbageNumbers();
    void savedLineEscapesTheFieldSeparator();
    void oldUnescapedSaveFilesStillLoad();
};

void tst_Testimony::statementReadsNamedFields()
{
    const Statement l_statement(baseFields("Hello", "7"));

    QCOMPARE(l_statement.message(), "Hello");
    QCOMPARE(l_statement.side(), "def");
    QCOMPARE(l_statement.charId(), 7);
    QCOMPARE(l_statement.textColor(), "0");
}

void tst_Testimony::malformedStatementReadsAsEmptyValues()
{
    // A statement short on fields answers with empty values, not a crash.
    const Statement l_statement(QStringList{"1", "pre"});

    QCOMPARE(l_statement.message(), QString());
    QCOMPARE(l_statement.side(), QString());
    QCOMPARE(l_statement.charId(), 0);

    Statement l_writable(QStringList{});
    l_writable.setMessage("text");
    l_writable.setTextColor("1");
    QCOMPARE(l_writable.message(), "text");
    QCOMPARE(l_writable.textColor(), "1");
}

void tst_Testimony::playbackFieldsBelongToNobody()
{
    const Statement l_statement(baseFields("Hello", "5"));
    const QStringList l_playback = l_statement.playbackFields();

    // The recorded char id 5 would read as player slot 5's own message.
    QCOMPARE(l_playback.at(8), "-1");
    // The stored statement itself keeps what was recorded.
    QCOMPARE(l_statement.charId(), 5);
}

void tst_Testimony::playbackFillsEmptyShownameFromTheFolderName()
{
    QStringList l_fields = baseFields("Hello");
    l_fields << ""
             << "-1"
             << ""
             << ""
             << "0&0"
             << ""
             << "0"
             << "0";
    const QStringList l_playback = Statement(l_fields).playbackFields();

    // With char id -1 the client shows the showname field, so an empty one
    // gets the character folder name.
    QCOMPARE(l_playback.at(15), "Phoenix");

    // A statement that carries a showname keeps it.
    l_fields[15] = "Nick";
    QCOMPARE(Statement(l_fields).playbackFields().at(15), "Nick");
}

void tst_Testimony::recordingAdvancesTheIndex()
{
    TestimonyRecorder l_recorder;
    QCOMPARE(l_recorder.statementIndex(), -1);

    l_recorder.record(Statement(baseFields("title")));
    l_recorder.record(Statement(baseFields("first")));

    QCOMPARE(l_recorder.statementIndex(), 1);
    QCOMPARE(l_recorder.statementCount(), 2);
    QCOMPARE(l_recorder.statementAt(1)->message(), "first");
    QVERIFY(!l_recorder.statementAt(2));
    QVERIFY(!l_recorder.statementAt(-1));
}

void tst_Testimony::editsIgnorePositionsThatDoNotExist()
{
    TestimonyRecorder l_recorder;
    l_recorder.record(Statement(baseFields("title")));
    l_recorder.record(Statement(baseFields("first")));

    l_recorder.replace(5, Statement(baseFields("lost")));
    l_recorder.replace(-1, Statement(baseFields("lost")));
    l_recorder.remove(5);
    l_recorder.remove(-1);
    l_recorder.insert(5, Statement(baseFields("lost")));
    l_recorder.insert(-1, Statement(baseFields("lost")));
    QCOMPARE(l_recorder.statementCount(), 2);
    QCOMPARE(l_recorder.statementIndex(), 1);

    l_recorder.insert(1, Statement(baseFields("inserted")));
    l_recorder.replace(2, Statement(baseFields("replaced")));
    QCOMPARE(l_recorder.statementCount(), 3);
    QCOMPARE(l_recorder.statementAt(1)->message(), "inserted");
    QCOMPARE(l_recorder.statementAt(2)->message(), "replaced");

    l_recorder.remove(1);
    QCOMPARE(l_recorder.statementCount(), 2);
    QCOMPARE(l_recorder.statementAt(1)->message(), "replaced");
}

void tst_Testimony::jumpNeedsAtLeastOneStatement()
{
    TestimonyRecorder l_recorder;
    QVERIFY(!l_recorder.jumpTo(1));

    // A lone title is still nothing to play back; the old code crashed here.
    l_recorder.record(Statement(baseFields("title")));
    QVERIFY(!l_recorder.jumpTo(1));
}

void tst_Testimony::jumpLoopsPastTheEndAndStaysAtTheFirst()
{
    TestimonyRecorder l_recorder;
    for (const QString &l_message : {"title", "first", "second", "third"}) {
        l_recorder.record(Statement(baseFields(l_message)));
    }

    auto l_jump = l_recorder.jumpTo(2);
    QCOMPARE(l_jump->first.message(), "second");
    QCOMPARE(l_jump->second, TestimonyRecorder::Playback::Ok);

    l_jump = l_recorder.jumpTo(4);
    QCOMPARE(l_jump->first.message(), "first");
    QCOMPARE(l_jump->second, TestimonyRecorder::Playback::Looped);
    QCOMPARE(l_recorder.statementIndex(), 1);

    l_jump = l_recorder.jumpTo(0);
    QCOMPARE(l_jump->first.message(), "first");
    QCOMPARE(l_jump->second, TestimonyRecorder::Playback::StayedAtFirst);
}

void tst_Testimony::clearForgetsEverything()
{
    TestimonyRecorder l_recorder;
    l_recorder.setState(TestimonyRecorder::State::Recording);
    l_recorder.record(Statement(baseFields("title")));

    l_recorder.clear();

    QCOMPARE(l_recorder.state(), TestimonyRecorder::State::Stopped);
    QCOMPARE(l_recorder.statementIndex(), -1);
    QCOMPARE(l_recorder.statementCount(), 0);
}

void tst_Testimony::savedLineRoundTripsThroughTheIcParser()
{
    const Statement l_original(baseFields("Hello world", "-1"));
    const std::optional<Statement> l_loaded = Statement::fromSavedLine(l_original.toSavedLine());

    QVERIFY(l_loaded);
    QCOMPARE(l_loaded->icFields(), l_original.icFields());
}

void tst_Testimony::savedLineKeepsThePairBlock()
{
    QStringList l_fields = baseFields("Paired", "-1");
    l_fields << "Nick"
             << "3^1"
             << "Edgeworth"
             << "point"
             << "10&0"
             << "-10&0"
             << "1"
             << "1";
    const std::optional<Statement> l_loaded = Statement::fromSavedLine(Statement(l_fields).toSavedLine());

    QVERIFY(l_loaded);
    QCOMPARE(l_loaded->icFields(), l_fields);
}

void tst_Testimony::shortSavedLineReadsAsNothing()
{
    QVERIFY(!Statement::fromSavedLine("way#too#short"));
    QVERIFY(!Statement::fromSavedLine(""));
}

void tst_Testimony::savedLineClearsTheCharId()
{
    // The saved id belonged to a player slot in a session long gone;
    // replaying it would name whoever holds the slot now.
    const std::optional<Statement> l_loaded = Statement::fromSavedLine(baseFields("Hello", "5").join("#"));

    QVERIFY(l_loaded);
    QCOMPARE(l_loaded->charId(), -1);
}

void tst_Testimony::savedLineNormalizesGarbageNumbers()
{
    QStringList l_fields = baseFields("Hello");
    l_fields[7] = "banana";
    l_fields[14] = "12rainbow";
    const std::optional<Statement> l_loaded = Statement::fromSavedLine(l_fields.join("#"));

    QVERIFY(l_loaded);
    QCOMPARE(l_loaded->icFields().at(7), "0");
    QCOMPARE(l_loaded->textColor(), "0");
}

void tst_Testimony::savedLineEscapesTheFieldSeparator()
{
    // A message holding the separator itself must not break the line apart
    // into extra fields when it is read back.
    const Statement l_original(baseFields("Exhibit #1, worth 100%", "-1"));
    const QString l_line = l_original.toSavedLine();
    QVERIFY(!l_line.contains("#1"));

    const std::optional<Statement> l_loaded = Statement::fromSavedLine(l_line);
    QVERIFY(l_loaded);
    QCOMPARE(l_loaded->message(), "Exhibit #1, worth 100%");
    QCOMPARE(l_loaded->icFields().size(), 15);
}

void tst_Testimony::oldUnescapedSaveFilesStillLoad()
{
    // Files written before this change hold plain unescaped fields.
    const std::optional<Statement> l_loaded = Statement::fromSavedLine("1#pre#Phoenix#normal#Hold it!#wit#sfx#0#3#0#2#0#0#1#0");

    QVERIFY(l_loaded);
    QCOMPARE(l_loaded->message(), "Hold it!");
    QCOMPARE(l_loaded->side(), "wit");
    QCOMPARE(l_loaded->charId(), -1);
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_Testimony)

#include "tst_testimony.moc"
