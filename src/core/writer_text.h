#pragma once

#include "akashi/log_writer.h"
#include "akashi_core_export.h"

#include <QDir>
#include <QString>

namespace akashi {

class LogService;

class AKASHI_CORE_EXPORT WriterText : public ILogWriter
{
  public:
    enum class Mode
    {
        Full,
        FullArea,
        Modcall
    };

    WriterText(Mode f_mode, LogService *f_log_service, int f_archive_after_days = 7);

    QString writerId() const override;
    void write(const LogEvent &f_event) override;
    void maintenance() override;

    void flushBuffer(const QString &f_area, const QList<LogEvent> &f_events);

  private:
    void writeToFile(const QString &f_path, const QString &f_text);
    void ensureDir(const QString &f_path);
    void archiveOldLogs();

    Mode m_mode;
    LogService *m_log_service;
    int m_archive_after_days;
    bool m_logs_dir_created = false;
    bool m_modcall_dir_created = false;
    bool m_write_failed = false;
};

} // namespace akashi
