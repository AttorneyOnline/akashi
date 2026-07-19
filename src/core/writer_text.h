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
    // Area names are operator-set (/createarea, /renamearea) and become part of
    // a log file name, so they must never carry a path separator or ".." out of
    // logs/. Fold anything that is not a plain name character to '_'.
    static QString safeAreaToken(const QString &f_area);
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
