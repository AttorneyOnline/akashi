#include "core/writer_text.h"

#include "core/log_service.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextStream>

namespace akashi {

WriterText::WriterText(Mode f_mode, LogService *f_log_service) :
    m_mode(f_mode),
    m_log_service(f_log_service)
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
        const QString l_area = f_event.area.isEmpty() ? QStringLiteral("SERVER") : f_event.area;
        writeToFile(QStringLiteral("logs/%1_%2.log").arg(l_area, l_date), l_formatted);
    }
}

void WriterText::flushBuffer(const QString &f_area, const QList<LogEvent> &f_events)
{
    ensureDir(QStringLiteral("logs/modcall"));
    const QString l_timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_hhmmss"));
    const QString l_path = QStringLiteral("logs/modcall/report_%1_%2.log").arg(f_area, l_timestamp);

    QString l_content;
    for (const auto &l_event : f_events) {
        l_content.append(m_log_service->formatEvent(l_event));
        l_content.append(QStringLiteral("\n"));
    }
    writeToFile(l_path, l_content);
}

void WriterText::writeToFile(const QString &f_path, const QString &f_text)
{
    QFile l_file(f_path);
    if (!l_file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        return;
    }
    QTextStream l_stream(&l_file);
    l_stream << f_text;
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
