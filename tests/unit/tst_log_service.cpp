// AI-generated: written by Claude.
#include "akashi/log_event.h"
#include "akashi/log_writer.h"
#include "core/log_service.h"

#include <QRegularExpression>
#include <QSemaphore>
#include <QTest>

namespace {

class TestWriter : public akashi::ILogWriter
{
  public:
    explicit TestWriter(const QString &f_id = QStringLiteral("test")) :
        id(f_id)
    {}

    QString writerId() const override { return id; }
    void write(const akashi::LogEvent &f_event) override
    {
        received.append(f_event);
        if (gate) {
            gate->release();
        }
    }

    QString id;
    QList<akashi::LogEvent> received;
    QSemaphore *gate = nullptr;
};

} // namespace

namespace tests {
namespace unittests {

class tst_LogService : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void bufferRoundTrip();
    void bufferEvictsOldest();
    void formatNamedPlaceholders();
    void formatAllEventTypes();
    void legacyPositionalMigration();
    void positionalLineagePickedByFieldCount();
    void forkDialectWarnsAndStaysLiteral();
    void loginTemplateRendersTheOutcome();
    void unknownPlaceholderWarnsAndStaysLiteral();
    void writerRegistration();
    void writerReceivesEvent();
    void writerFormatsOnTheWorkerThread();
    void multipleWritersReceive();
    void unregisterByOwner();
    void unregisterUnknownOwnerKeepsWriters();
    void sameWriterIdRegisteredTwiceIsRefused();
    void formatMissingFieldsEmpty();
    void formatUnknownTypeIsEmpty();
    void emptyAreaBucketsUnderServer();
    void customTemplate();
};

void tst_LogService::bufferRoundTrip()
{
    akashi::LogService l_service(nullptr, 100);

    akashi::LogEvent l_event;
    l_event.type = akashi::log_type::IC;
    l_event.area = QStringLiteral("Courtroom");
    l_event.char_name = QStringLiteral("Phoenix");
    l_event.message = QStringLiteral("Objection!");
    l_service.log(l_event);

    auto l_events = l_service.recentEvents(QStringLiteral("Courtroom"), 10);
    QCOMPARE(l_events.size(), 1);
    QCOMPARE(l_events.first().char_name, QStringLiteral("Phoenix"));
    QCOMPARE(l_events.first().message, QStringLiteral("Objection!"));
    QVERIFY(l_events.first().timestamp > 0);
}

void tst_LogService::bufferEvictsOldest()
{
    akashi::LogService l_service(nullptr, 3);

    for (int i = 0; i < 5; ++i) {
        akashi::LogEvent l_event;
        l_event.type = akashi::log_type::OOC;
        l_event.area = QStringLiteral("Lobby");
        l_event.message = QString::number(i);
        l_service.log(l_event);
    }

    auto l_events = l_service.recentEvents(QStringLiteral("Lobby"), 0);
    QCOMPARE(l_events.size(), 3);
    QCOMPARE(l_events.first().message, QStringLiteral("2"));
    QCOMPARE(l_events.last().message, QStringLiteral("4"));
}

void tst_LogService::formatNamedPlaceholders()
{
    akashi::LogService l_service(nullptr);

    akashi::LogEvent l_event;
    l_event.timestamp = 1000;
    l_event.type = akashi::log_type::CMD;
    l_event.area = QStringLiteral("Basement");
    l_event.char_name = QStringLiteral("Phoenix Wright");
    l_event.ooc_name = QStringLiteral("Nick");
    l_event.ipid = QStringLiteral("abcd1234");
    l_event.message = QStringLiteral("ban");
    l_event.args = QStringLiteral("badguy spamming");

    QString l_formatted = l_service.formatEvent(l_event);
    QVERIFY(l_formatted.contains(QStringLiteral("Basement")));
    QVERIFY(l_formatted.contains(QStringLiteral("CMD")));
    QVERIFY(l_formatted.contains(QStringLiteral("Phoenix Wright")));
    QVERIFY(l_formatted.contains(QStringLiteral("/ban")));
    QVERIFY(l_formatted.contains(QStringLiteral("badguy spamming")));
}

void tst_LogService::formatAllEventTypes()
{
    akashi::LogService l_service(nullptr);

    const QStringList l_types = {
        akashi::log_type::IC, akashi::log_type::OOC,
        akashi::log_type::Music, akashi::log_type::Login,
        akashi::log_type::CMD, akashi::log_type::Kick,
        akashi::log_type::Ban, akashi::log_type::Modcall,
        akashi::log_type::Connect};

    for (const QString &l_type : l_types) {
        akashi::LogEvent l_event;
        l_event.timestamp = 1000;
        l_event.type = l_type;
        l_event.area = QStringLiteral("Room");
        l_event.char_name = QStringLiteral("Test");
        QString l_result = l_service.formatEvent(l_event);
        QVERIFY2(!l_result.isEmpty(), qPrintable("No template for type: " + l_type));
    }
}

void tst_LogService::legacyPositionalMigration()
{
    akashi::LogService l_service(nullptr);

    // A shape no default template has, so the assertion can only pass
    // through the migrated registration.
    l_service.registerTemplate(akashi::log_type::Kick,
                               QStringLiteral("K|%2|%3|%4"));

    akashi::LogEvent l_event;
    l_event.timestamp = 1000;
    l_event.type = akashi::log_type::Kick;
    l_event.moderator = QStringLiteral("Admin");
    l_event.target_ipid = QStringLiteral("beef1234");
    l_event.message = QStringLiteral("spamming");

    QCOMPARE(l_service.formatEvent(l_event), QStringLiteral("K|Admin|beef1234|spamming"));
}

void tst_LogService::positionalLineagePickedByFieldCount()
{
    akashi::LogService l_service(nullptr);

    akashi::LogEvent l_event;
    l_event.timestamp = 1000;
    l_event.type = akashi::log_type::IC;
    l_event.area = QStringLiteral("Basement");
    l_event.char_name = QStringLiteral("Phoenix");
    l_event.ooc_name = QStringLiteral("Nick");
    l_event.ipid = QStringLiteral("abcd1234");
    l_event.client_id = QStringLiteral("3");
    l_event.message = QStringLiteral("Objection!");

    // The 1.x logtext.ini layout tops out at %6: char and ooc come before
    // ipid and area, and %6 is the message.
    l_service.registerTemplate(akashi::log_type::IC,
                               QStringLiteral("[%1][%5][IC][%2(%3)][%4]%6"));
    QString l_old = l_service.formatEvent(l_event);
    QVERIFY(l_old.contains(QStringLiteral("[Basement][IC][Phoenix(Nick)][abcd1234]Objection!")));

    // The later sample layout uses %7 and carries the client id.
    l_service.registerTemplate(akashi::log_type::IC,
                               QStringLiteral("[%1][%2][IC]{%3}[%4]%5(%6): %7"));
    QString l_new = l_service.formatEvent(l_event);
    QVERIFY(l_new.contains(QStringLiteral("[Basement][IC]{abcd1234}[3]Phoenix(Nick): Objection!")));
}

void tst_LogService::forkDialectWarnsAndStaysLiteral()
{
    akashi::LogService l_service(nullptr);

    // Fork configs carry angle-bracket placeholders. Akashi does not
    // migrate foreign dialects: the template loads as written, and every
    // unknown name is called out so the operator knows what to fix.
    QTest::ignoreMessage(QtWarningMsg,
                         QRegularExpression(QStringLiteral("unknown placeholder <time>")));
    QTest::ignoreMessage(QtWarningMsg,
                         QRegularExpression(QStringLiteral("unknown placeholder <passed>")));
    l_service.registerTemplate(akashi::log_type::Login,
                               QStringLiteral("[<time>][LOGIN]: <passed> {message}"));

    akashi::LogEvent l_event;
    l_event.timestamp = 1000;
    l_event.type = akashi::log_type::Login;
    l_event.message = QStringLiteral("hello");

    QCOMPARE(l_service.formatEvent(l_event), QStringLiteral("[<time>][LOGIN]: <passed> hello"));
}

void tst_LogService::loginTemplateRendersTheOutcome()
{
    akashi::LogService l_service(nullptr);

    akashi::LogEvent l_event;
    l_event.timestamp = 1000;
    l_event.type = akashi::log_type::Login;
    l_event.ipid = QStringLiteral("abcd1234");
    l_event.char_name = QStringLiteral("Phoenix");
    l_event.success = false;
    QVERIFY(l_service.formatEvent(l_event).contains(QStringLiteral("[LOGIN][FAILED]")));

    l_event.success = true;
    QVERIFY(l_service.formatEvent(l_event).contains(QStringLiteral("[LOGIN][SUCCESS]")));

    // The legacy positional login maps %2 to the outcome, as its own
    // documentation always said.
    l_service.registerTemplate(akashi::log_type::Login,
                               QStringLiteral("[%1][LOGIN][%2][%3][%4(%5)]"));
    l_event.success = false;
    QVERIFY(l_service.formatEvent(l_event).contains(QStringLiteral("[LOGIN][FAILED]")));
}

void tst_LogService::unknownPlaceholderWarnsAndStaysLiteral()
{
    akashi::LogService l_service(nullptr);

    QTest::ignoreMessage(QtWarningMsg,
                         QRegularExpression(QStringLiteral("unknown placeholder \\{char\\}")));
    l_service.registerTemplate(akashi::log_type::IC,
                               QStringLiteral("[{char}] {message}"));

    akashi::LogEvent l_event;
    l_event.timestamp = 1000;
    l_event.type = akashi::log_type::IC;
    l_event.char_name = QStringLiteral("Phoenix");
    l_event.message = QStringLiteral("Objection!");

    // The template still loads; the unknown token lands as written.
    QCOMPARE(l_service.formatEvent(l_event), QStringLiteral("[{char}] Objection!"));
}

void tst_LogService::writerRegistration()
{
    akashi::LogService l_service(nullptr);

    auto l_writer = std::make_shared<TestWriter>();
    l_service.registerWriter(l_writer, QStringLiteral("test-plugin"));

    l_service.unregisterAll(QStringLiteral("test-plugin"));
}

void tst_LogService::writerReceivesEvent()
{
    akashi::LogService l_service(nullptr);

    QSemaphore l_sem;
    auto l_writer = std::make_shared<TestWriter>();
    l_writer->gate = &l_sem;
    l_service.registerWriter(l_writer, QStringLiteral("test"));

    akashi::LogEvent l_event;
    l_event.type = akashi::log_type::IC;
    l_event.area = QStringLiteral("Lobby");
    l_event.message = QStringLiteral("Hello");
    l_service.log(l_event);

    QVERIFY(l_sem.tryAcquire(1, 5000));
    QCOMPARE(l_writer->received.size(), 1);
    QCOMPARE(l_writer->received.first().message, QStringLiteral("Hello"));
}

void tst_LogService::writerFormatsOnTheWorkerThread()
{
    akashi::LogService l_service(nullptr);

    // WriterText's exact pattern: rendering the line from inside write(),
    // which the service calls on its worker thread. This used to trip
    // formatEvent's main-thread assert, so full-mode logs wrote nothing.
    class FormattingWriter : public akashi::ILogWriter
    {
      public:
        QString writerId() const override { return QStringLiteral("formatting"); }
        void write(const akashi::LogEvent &f_event) override
        {
            formatted = service->formatEvent(f_event);
            gate->release();
        }
        akashi::LogService *service = nullptr;
        QSemaphore *gate = nullptr;
        QString formatted;
    };

    QSemaphore l_sem;
    auto l_writer = std::make_shared<FormattingWriter>();
    l_writer->service = &l_service;
    l_writer->gate = &l_sem;
    l_service.registerWriter(l_writer, QStringLiteral("test"));

    akashi::LogEvent l_event;
    l_event.timestamp = 1000;
    l_event.type = akashi::log_type::IC;
    l_event.area = QStringLiteral("Lobby");
    l_event.char_name = QStringLiteral("Phoenix");
    l_event.message = QStringLiteral("Objection!");
    l_service.log(l_event);

    QVERIFY(l_sem.tryAcquire(1, 5000));
    QVERIFY(l_writer->formatted.contains(QStringLiteral("Phoenix")));
    QVERIFY(l_writer->formatted.contains(QStringLiteral("Objection!")));
}

void tst_LogService::multipleWritersReceive()
{
    akashi::LogService l_service(nullptr);

    QSemaphore l_sem;
    auto l_writer1 = std::make_shared<TestWriter>(QStringLiteral("test-1"));
    auto l_writer2 = std::make_shared<TestWriter>(QStringLiteral("test-2"));
    l_writer2->gate = &l_sem;
    l_service.registerWriter(l_writer1, QStringLiteral("a"));
    l_service.registerWriter(l_writer2, QStringLiteral("b"));

    akashi::LogEvent l_event;
    l_event.type = akashi::log_type::OOC;
    l_event.area = QStringLiteral("Lobby");
    l_event.message = QStringLiteral("test");
    l_service.log(l_event);

    QVERIFY(l_sem.tryAcquire(1, 5000));
    QCOMPARE(l_writer1->received.size(), 1);
    QCOMPARE(l_writer2->received.size(), 1);
}

void tst_LogService::unregisterByOwner()
{
    akashi::LogService l_service(nullptr);

    QSemaphore l_sem;
    auto l_kept = std::make_shared<TestWriter>(QStringLiteral("kept"));
    auto l_removed = std::make_shared<TestWriter>(QStringLiteral("removed"));
    l_kept->gate = &l_sem;
    l_service.registerWriter(l_removed, QStringLiteral("plugin-a"));
    l_service.registerWriter(l_kept, QStringLiteral("plugin-b"));

    l_service.unregisterAll(QStringLiteral("plugin-a"));

    akashi::LogEvent l_event;
    l_event.type = akashi::log_type::IC;
    l_event.area = QStringLiteral("Lobby");
    l_service.log(l_event);

    QVERIFY(l_sem.tryAcquire(1, 5000));
    QCOMPARE(l_kept->received.size(), 1);
    QCOMPARE(l_removed->received.size(), 0);
}

void tst_LogService::unregisterUnknownOwnerKeepsWriters()
{
    akashi::LogService l_service(nullptr);

    QSemaphore l_sem;
    auto l_writer = std::make_shared<TestWriter>();
    l_writer->gate = &l_sem;
    l_service.registerWriter(l_writer, QStringLiteral("plugin-a"));

    l_service.unregisterAll(QStringLiteral("plugin-b"));

    akashi::LogEvent l_event;
    l_event.type = akashi::log_type::IC;
    l_event.area = QStringLiteral("Lobby");
    l_service.log(l_event);

    QVERIFY(l_sem.tryAcquire(1, 5000));
    QCOMPARE(l_writer->received.size(), 1);
}

void tst_LogService::sameWriterIdRegisteredTwiceIsRefused()
{
    // A known writerId registers once: the second owner is refused with a
    // warning naming the id and both owners, so events arrive once.
    akashi::LogService l_service(nullptr);

    QSemaphore l_sem;
    auto l_writer = std::make_shared<TestWriter>();
    l_writer->gate = &l_sem;
    l_service.registerWriter(l_writer, QStringLiteral("plugin-a"));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("test.*plugin-b.*plugin-a")));
    l_service.registerWriter(l_writer, QStringLiteral("plugin-b"));

    akashi::LogEvent l_event;
    l_event.type = akashi::log_type::IC;
    l_event.area = QStringLiteral("Lobby");
    l_service.log(l_event);

    QVERIFY(l_sem.tryAcquire(1, 5000));
    QCOMPARE(l_writer->received.size(), 1);

    // The refused owner holds nothing, so its sweep removes nothing.
    l_service.unregisterAll(QStringLiteral("plugin-b"));
    l_service.log(l_event);
    QVERIFY(l_sem.tryAcquire(1, 5000));
    QCOMPARE(l_writer->received.size(), 2);

    // The first owner's sweep removes the writer for good. A fresh witness
    // writer proves the worker handled the last event without it.
    l_service.unregisterAll(QStringLiteral("plugin-a"));
    QSemaphore l_witness_sem;
    auto l_witness = std::make_shared<TestWriter>(QStringLiteral("witness"));
    l_witness->gate = &l_witness_sem;
    l_service.registerWriter(l_witness, QStringLiteral("plugin-c"));
    l_service.log(l_event);
    QVERIFY(l_witness_sem.tryAcquire(1, 5000));
    QCOMPARE(l_writer->received.size(), 2);
}

void tst_LogService::formatMissingFieldsEmpty()
{
    akashi::LogService l_service(nullptr);

    akashi::LogEvent l_event;
    l_event.timestamp = 1000;
    l_event.type = akashi::log_type::IC;

    QString l_formatted = l_service.formatEvent(l_event);
    QVERIFY(!l_formatted.contains(QStringLiteral("{char_name}")));
    QVERIFY(!l_formatted.contains(QStringLiteral("{area}")));
}

void tst_LogService::formatUnknownTypeIsEmpty()
{
    // An event type nobody registered a template for formats to nothing.
    akashi::LogService l_service(nullptr);

    akashi::LogEvent l_event;
    l_event.timestamp = 1000;
    l_event.type = QStringLiteral("no_such_type");
    l_event.message = QStringLiteral("lost");

    QCOMPARE(l_service.formatEvent(l_event), QString());
}

void tst_LogService::emptyAreaBucketsUnderServer()
{
    akashi::LogService l_service(nullptr, 100);

    akashi::LogEvent l_event;
    l_event.type = akashi::log_type::Connect;
    l_event.message = QStringLiteral("hello");
    l_service.log(l_event);

    // An area-less event lands in the SERVER bucket; asking for the empty
    // area or any unknown one reads nothing.
    QCOMPARE(l_service.recentEvents(QStringLiteral("SERVER"), 0).size(), 1);
    QVERIFY(l_service.recentEvents(QString(), 0).isEmpty());
    QVERIFY(l_service.recentEvents(QStringLiteral("Atlantis"), 0).isEmpty());
}

void tst_LogService::customTemplate()
{
    akashi::LogService l_service(nullptr);

    l_service.registerTemplate(QStringLiteral("custom_plugin"),
                               QStringLiteral("{char_name} did {message} in {area}"));

    akashi::LogEvent l_event;
    l_event.timestamp = 1000;
    l_event.type = QStringLiteral("custom_plugin");
    l_event.char_name = QStringLiteral("Maya");
    l_event.message = QStringLiteral("channeling");
    l_event.area = QStringLiteral("Temple");

    QString l_formatted = l_service.formatEvent(l_event);
    QCOMPARE(l_formatted, QStringLiteral("Maya did channeling in Temple"));
}

} // namespace unittests
} // namespace tests

QTEST_GUILESS_MAIN(tests::unittests::tst_LogService)

#include "tst_log_service.moc"
