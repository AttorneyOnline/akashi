#include "core/writer_text.h"

#include "akashi/logging_categories.h"
#include "core/log_service.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

namespace akashi {

WriterText::WriterText(Mode f_mode, LogService *f_log_service, int f_archive_after_days) :
    m_mode(f_mode),
    m_log_service(f_log_service),
    m_archive_after_days(f_archive_after_days)
{
}

QString WriterText::writerId() const
{
    return QStringLiteral("akashi.writer.text");
}

void WriterText::write(const LogEvent &f_event)
{
    if (m_mode == Mode::Modcall) {
        return;
    }

    const QString l_formatted = m_log_service->formatEvent(f_event) + QStringLiteral("\n");
    const QString l_date = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));

    if (m_mode == Mode::Full) {
        ensureDir(QStringLiteral("logs"));
        writeToFile(QStringLiteral("logs/%1.log").arg(l_date), l_formatted);
    }
    else {
        ensureDir(QStringLiteral("logs"));
        const QString l_area = safeAreaToken(f_event.area);
        writeToFile(QStringLiteral("logs/%1_%2.log").arg(l_area, l_date), l_formatted);
    }
}

QString WriterText::safeAreaToken(const QString &f_area)
{
    QString l_token;
    l_token.reserve(f_area.size());
    for (const QChar l_ch : f_area) {
        if (l_ch.isLetterOrNumber() || l_ch == QLatin1Char('_') || l_ch == QLatin1Char('-')) {
            l_token.append(l_ch);
        }
        else {
            l_token.append(QLatin1Char('_'));
        }
    }
    // Empty (or all-separator) names fall back to the same label an
    // area-less event already used.
    return l_token.isEmpty() ? QStringLiteral("SERVER") : l_token;
}

void WriterText::flushBuffer(const QString &f_area, const QList<LogEvent> &f_events)
{
    ensureDir(QStringLiteral("logs/modcall"));
    const QString l_timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_hhmmss"));
    const QString l_path = QStringLiteral("logs/modcall/report_%1_%2.log").arg(safeAreaToken(f_area), l_timestamp);

    QString l_content;
    for (const auto &l_event : f_events) {
        l_content.append(m_log_service->formatEvent(l_event));
        l_content.append(QStringLiteral("\n"));
    }
    writeToFile(l_path, l_content);
}

void WriterText::writeToFile(const QString &f_path, const QString &f_text)
{
    // The latch keeps a disk outage at one warning, not one per log line.
    QFile l_file(f_path);
    if (!l_file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        if (!m_write_failed) {
            qCWarning(akashiLog) << "WriterText: cannot open" << f_path << "-" << l_file.errorString();
            m_write_failed = true;
        }
        return;
    }
    QTextStream l_stream(&l_file);
    l_stream << f_text;
    l_stream.flush();
    if (l_stream.status() != QTextStream::Ok) {
        if (!m_write_failed) {
            qCWarning(akashiLog) << "WriterText: write failed on" << f_path << "-" << l_file.errorString();
            m_write_failed = true;
        }
        return;
    }
    if (m_write_failed) {
        qCInfo(akashiLog) << "WriterText: writing resumed on" << f_path;
        m_write_failed = false;
    }
}

void WriterText::maintenance()
{
    archiveOldLogs();
}

void WriterText::archiveOldLogs()
{
    QDir l_logs_dir(QStringLiteral("logs"));
    if (!l_logs_dir.exists()) {
        return;
    }

    const QDate l_cutoff = QDate::currentDate().addDays(-m_archive_after_days);
    const QStringList l_files = l_logs_dir.entryList({QStringLiteral("*.log")}, QDir::Files);
    if (l_files.isEmpty()) {
        return;
    }

    static const QRegularExpression s_date_pattern(QStringLiteral("(\\d{4}-\\d{2}-\\d{2})\\.log$"));
    QStringList l_to_archive;
    for (const QString &l_file : l_files) {
        QRegularExpressionMatch l_match = s_date_pattern.match(l_file);
        if (!l_match.hasMatch()) {
            continue;
        }
        QDate l_date = QDate::fromString(l_match.captured(1), QStringLiteral("yyyy-MM-dd"));
        if (l_date.isValid() && l_date < l_cutoff) {
            l_to_archive.append(l_file);
        }
    }

    if (l_to_archive.isEmpty()) {
        return;
    }

    QDir l_archive_dir(QStringLiteral("logs/archive"));
    if (!l_archive_dir.exists()) {
        l_archive_dir.mkpath(QStringLiteral("."));
    }

    int l_moved = 0;
    for (const QString &l_file : l_to_archive) {
        const QString l_src = l_logs_dir.filePath(l_file);
        const QString l_dst = l_archive_dir.filePath(l_file);
        if (QFile::rename(l_src, l_dst)) {
            ++l_moved;
        }
    }
    if (l_moved > 0) {
        qCInfo(akashiLog) << "WriterText: archived" << l_moved << "log files older than" << m_archive_after_days << "days";
    }
}

void WriterText::ensureDir(const QString &f_path)
{
    if (f_path == QStringLiteral("logs") && m_logs_dir_created) {
        return;
    }
    if (f_path == QStringLiteral("logs/modcall") && m_modcall_dir_created) {
        return;
    }

    QDir l_dir(f_path);
    if (!l_dir.exists()) {
        l_dir.mkpath(QStringLiteral("."));
    }

    if (f_path == QStringLiteral("logs")) {
        m_logs_dir_created = true;
    }
    if (f_path == QStringLiteral("logs/modcall")) {
        m_modcall_dir_created = true;
    }
}

} // namespace akashi
