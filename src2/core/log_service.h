#pragma once

#include "akashi/log_event.h"
#include "akashi/log_writer.h"
#include "akashi/service.h"
#include "akashi_core_export.h"

#include <QHash>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QWaitCondition>

#include <atomic>
#include <memory>

class QThread;

namespace akashi {

class ConfigStore;

class AKASHI_CORE_EXPORT LogService : public QObject, public IService
{
    Q_OBJECT

  public:
    explicit LogService(ConfigStore *f_config_store, int f_buffer_limit = 500, QObject *parent = nullptr);
    ~LogService() override;

    QString serviceId() const override;
    ServiceVersion serviceVersion() const override;

    void log(LogEvent f_event);

    QList<LogEvent> recentEvents(const QString &f_area, int f_count) const;

    QString formatEvent(const LogEvent &f_event) const;

    void registerWriter(std::shared_ptr<ILogWriter> f_writer, const QString &f_owner_id = {});
    void unregisterAll(const QString &f_owner_id);

    void registerTemplate(const QString &f_type, const QString &f_tmpl);

  public Q_SLOTS:
    void reloadTemplates();
    void runWriterMaintenance();

  private:
    void stopWorker();
    void workerLoop();

    QString applyTemplate(const QString &f_tmpl, const LogEvent &f_event) const;
    void loadTemplates();
    static QString migratePositionalTemplate(const QString &f_key, const QString &f_tmpl);

    ConfigStore *m_config_store;
    int m_buffer_limit;

    QHash<QString, QList<LogEvent>> m_buffers;
    QHash<QString, QString> m_templates;

    struct WriterEntry
    {
        std::shared_ptr<ILogWriter> writer;
        QString owner;
    };

    QMutex m_mutex;
    QWaitCondition m_condition;
    QQueue<LogEvent> m_queue;
    QList<WriterEntry> m_writers;

    QThread *m_worker = nullptr;
    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_run_maintenance{false};
};

} // namespace akashi

