// AI-generated: written by Claude.
#include "akashi/log_event.h"
#include "akashi/log_writer.h"
#include "core/log_service.h"

#include <QSemaphore>
#include <QTest>

namespace {

class TestWriter : public akashi::ILogWriter
{
  public:
    QString writerId() const override { return QStringLiteral("test"); }
    void write(const akashi::LogEvent &f_event) override
    {
        received.append(f_event);
        if (gate) {
            gate->release();
        }
    }

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
    void writerRegistration();
    void writerReceivesEvent();
    void multipleWritersReceive();
    void unregisterByOwner();
    void formatMissingFieldsEmpty();
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
        akashi::log_type::Connect
    };

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

    l_service.registerTemplate(akashi::log_type::Kick,
        QStringLiteral("[%1][%2][KICK][%3]: %4"));

    l_service.reloadTemplates();

    akashi::LogEvent l_event;
    l_event.timestamp = 1000;
    l_event.type = akashi::log_type::Kick;
    l_event.moderator = QStringLiteral("Admin");
    l_event.target_ipid = QStringLiteral("beef1234");
    l_event.message = QStringLiteral("spamming");

    QString l_formatted = l_service.formatEvent(l_event);
    QVERIFY(l_formatted.contains(QStringLiteral("Admin")));
    QVERIFY(l_formatted.contains(QStringLiteral("beef1234")));
    QVERIFY(l_formatted.contains(QStringLiteral("spamming")));
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

void tst_LogService::multipleWritersReceive()
{
    akashi::LogService l_service(nullptr);

    QSemaphore l_sem;
    auto l_writer1 = std::make_shared<TestWriter>();
    auto l_writer2 = std::make_shared<TestWriter>();
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
    auto l_kept = std::make_shared<TestWriter>();
    auto l_removed = std::make_shared<TestWriter>();
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
