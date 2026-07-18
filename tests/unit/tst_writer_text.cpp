// AI-generated: written by Claude.
#include "akashi/log_event.h"
#include "core/log_service.h"
#include "core/writer_text.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

namespace tests {
namespace unittests {

class tst_WriterText : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void failedWritesWarnOnceAndRecover();
};

void tst_WriterText::failedWritesWarnOnceAndRecover()
{
    QTemporaryDir l_dir;
    QVERIFY(l_dir.isValid());
    const QString l_old_cwd = QDir::currentPath();
    QVERIFY(QDir::setCurrent(l_dir.path()));

    // A file squatting on the logs directory name makes every open fail.
    {
        QFile l_squatter(QStringLiteral("logs"));
        QVERIFY(l_squatter.open(QIODevice::WriteOnly));
    }

    akashi::LogService l_service(nullptr);
    akashi::WriterText l_writer(akashi::WriterText::Mode::Full, &l_service);

    akashi::LogEvent l_event;
    l_event.timestamp = 1000;
    l_event.type = akashi::log_type::IC;
    l_event.area = QStringLiteral("Lobby");
    l_event.char_name = QStringLiteral("Phoenix");
    l_event.message = QStringLiteral("Objection!");

    // The first failure warns; the repeat stays quiet behind the latch.
    QTest::failOnWarning(QRegularExpression(QStringLiteral(".*")));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("WriterText: cannot open")));
    l_writer.write(l_event);
    l_writer.write(l_event);

    // Clearing the blockage lets the writer resume, and say so.
    QVERIFY(QFile::remove(QStringLiteral("logs")));
    QVERIFY(QDir().mkpath(QStringLiteral("logs")));
    QTest::ignoreMessage(QtInfoMsg, QRegularExpression(QStringLiteral("WriterText: writing resumed")));
    l_writer.write(l_event);

    const QStringList l_logs = QDir(QStringLiteral("logs")).entryList({QStringLiteral("*.log")}, QDir::Files);
    QCOMPARE(l_logs.size(), 1);
    QFile l_log(QStringLiteral("logs/") + l_logs.first());
    QVERIFY(l_log.open(QIODevice::ReadOnly));
    QVERIFY(QString::fromUtf8(l_log.readAll()).contains(QStringLiteral("Objection!")));

    QVERIFY(QDir::setCurrent(l_old_cwd));
}

} // namespace unittests
} // namespace tests

QTEST_GUILESS_MAIN(tests::unittests::tst_WriterText)

#include "tst_writer_text.moc"
