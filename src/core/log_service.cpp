#include "core/log_service.h"

#include "akashi/config_store.h"
#include "akashi/logging_categories.h"
#include "akashi/thread_assert.h"

#include <QDateTime>
#include <QDebug>
#include <QRegularExpression>
#include <QSettings>
#include <QThread>

#include <utility>

namespace akashi {

static const QHash<QString, QString> &defaultTemplates()
{
    static const QHash<QString, QString> s_templates = {
        {"ic", QStringLiteral("[{timestamp}][{char_name}][IC][{area}({ipid})][{client_id}]{message}")},
        {"ooc", QStringLiteral("[{timestamp}][{char_name}][OOC][{area}({ipid})][{client_id}]{message}")},
        {"music", QStringLiteral("[{timestamp}][{char_name}][MUSIC][{ooc_name}({ipid})][{area}]{message}")},
        {"login", QStringLiteral("[{timestamp}][LOGIN][{success}][{ipid}][{char_name}({ooc_name})]")},
        {"cmd", QStringLiteral("[{timestamp}][{area}][CMD][{ipid}][{char_name}({ooc_name})]/{message} {args}")},
        {"kick", QStringLiteral("[{timestamp}][{moderator}][KICK][{target_ipid}]: {message}")},
        {"ban", QStringLiteral("[{timestamp}][{moderator}][BAN][{target_ipid}][{duration}]: {message}")},
        {"modcall", QStringLiteral("[{timestamp}][{area}][MODCALL][{ipid}][{client_id}][{char_name}({ooc_name})]")},
        {"connect", QStringLiteral("[{timestamp}][CONNECT][{ipid}][{target_ipid}][{hwid}]")},
    };
    return s_templates;
}

// A template is legacy-positional if it uses any %N field, not just %1 -
// an operator may well have dropped the leading timestamp field.
static bool isPositionalTemplate(const QString &f_tmpl)
{
    static const QRegularExpression s_positional(QStringLiteral("%[1-9]"));
    return f_tmpl.contains(s_positional);
}

// Every placeholder applyTemplate substitutes; anything else in a template
// lands in the log line as written, so loading warns about unknown tokens.
static const QStringList &knownPlaceholders()
{
    static const QStringList s_placeholders = {
        QStringLiteral("timestamp"), QStringLiteral("area"), QStringLiteral("char_name"),
        QStringLiteral("ooc_name"), QStringLiteral("ipid"), QStringLiteral("client_id"),
        QStringLiteral("message"), QStringLiteral("args"), QStringLiteral("moderator"),
        QStringLiteral("target_ipid"), QStringLiteral("duration"), QStringLiteral("hwid"),
        QStringLiteral("success")};
    return s_placeholders;
}

// Two positional lineages exist in migrated configs: the 1.x logtext.ini
// layout (its comments document the field meanings) and the later sample
// layout that added client ids. The highest positional a template uses
// picks the lineage. The login layout is identical in both, and its %2
// always meant the login outcome.
struct PositionalLineage
{
    int field_count;
    QStringList fields;
};

static const QHash<QString, QList<PositionalLineage>> &positionalMigration()
{
    static const QHash<QString, QList<PositionalLineage>> s_migration = {
        {"ic", {{6, {"{timestamp}", "{char_name}", "{ooc_name}", "{ipid}", "{area}", "{message}"}}, {7, {"{timestamp}", "{area}", "{ipid}", "{client_id}", "{char_name}", "{ooc_name}", "{message}"}}}},
        {"ooc", {{6, {"{timestamp}", "{char_name}", "{ooc_name}", "{ipid}", "{area}", "{message}"}}, {7, {"{timestamp}", "{area}", "{ipid}", "{client_id}", "{char_name}", "{ooc_name}", "{message}"}}}},
        {"music", {{6, {"{timestamp}", "{char_name}", "{ooc_name}", "{ipid}", "{area}", "{message}"}}}},
        {"login", {{5, {"{timestamp}", "{success}", "{ipid}", "{char_name}", "{ooc_name}"}}}},
        {"cmd", {{7, {"{timestamp}", "{area}", "{char_name}", "{ooc_name}", "{message}", "{args}", "{ipid}"}}}},
        {"kick", {{4, {"{timestamp}", "{moderator}", "{target_ipid}", "{message}"}}}},
        {"ban", {{5, {"{timestamp}", "{moderator}", "{target_ipid}", "{duration}", "{message}"}}}},
        {"modcall", {{5, {"{timestamp}", "{area}", "{char_name}", "{ooc_name}", "{ipid}"}}, {6, {"{timestamp}", "{area}", "{ipid}", "{client_id}", "{char_name}", "{ooc_name}"}}}},
        {"connect", {{4, {"{timestamp}", "{ipid}", "{target_ipid}", "{hwid}"}}}},
    };
    return s_migration;
}

LogService::LogService(ConfigStore *f_config_store, int f_buffer_limit, QObject *parent) :
    QObject(parent),
    m_config_store(f_config_store),
    m_buffer_limit(f_buffer_limit)
{
    m_templates = defaultTemplates();
    loadTemplates();

    m_worker = QThread::create([this]() { workerLoop(); });
    m_worker->start();
}

LogService::~LogService()
{
    stopWorker();
}

QString LogService::serviceId() const
{
    return QStringLiteral("akashi.log");
}

ServiceVersion LogService::serviceVersion() const
{
    return {1, 0, 0};
}

void LogService::log(LogEvent f_event)
{
    // Main-thread only, like recentEvents(): the two share m_buffers with no
    // lock. The mutex below is the hand-off to the writer thread - this is the
    // producer side, the worker is the consumer. A plugin logging from a
    // worker thread must marshal the call to the main thread first.
    AKASHI_ASSERT_THREAD_AFFINITY();
    if (f_event.timestamp == 0) {
        f_event.timestamp = QDateTime::currentMSecsSinceEpoch();
    }

    const QString l_area = f_event.area.isEmpty() ? QStringLiteral("SERVER") : f_event.area;
    auto &l_buffer = m_buffers[l_area];
    while (l_buffer.size() >= m_buffer_limit) {
        l_buffer.removeFirst();
    }
    l_buffer.append(f_event);

    {
        QMutexLocker l_lock(&m_mutex);
        m_queue.enqueue(std::move(f_event));
    }
    m_condition.wakeOne();
}

QList<LogEvent> LogService::recentEvents(const QString &f_area, int f_count) const
{
    AKASHI_ASSERT_THREAD_AFFINITY();
    auto l_it = m_buffers.constFind(f_area);
    if (l_it == m_buffers.constEnd()) {
        return {};
    }
    const auto &l_buffer = l_it.value();
    if (f_count <= 0 || f_count >= l_buffer.size()) {
        return l_buffer;
    }
    return l_buffer.mid(l_buffer.size() - f_count);
}

QString LogService::formatEvent(const LogEvent &f_event) const
{
    // Writers render on the worker thread, so this read has no thread
    // affinity; the mutex is what keeps a /reload from racing it.
    QString l_tmpl;
    {
        QMutexLocker l_lock(&m_mutex);
        auto l_it = m_templates.constFind(f_event.type);
        if (l_it == m_templates.constEnd()) {
            return {};
        }
        l_tmpl = l_it.value();
    }
    return applyTemplate(l_tmpl, f_event);
}

void LogService::registerWriter(std::shared_ptr<ILogWriter> f_writer, const QString &f_owner_id)
{
    AKASHI_ASSERT_THREAD_AFFINITY();
    // Registration is main-thread only; the mutex is what the worker
    // snapshots the list under, so the dedup check reads under it too.
    QMutexLocker l_lock(&m_mutex);
    const QString l_writer_id = f_writer->writerId();
    for (const auto &l_entry : std::as_const(m_writers)) {
        if (l_entry.writer->writerId() == l_writer_id) {
            // A writer id registers once, or every event would be written
            // twice; the first owner keeps the writer.
            qCWarning(akashiLog) << "Refused log writer" << l_writer_id << "from" << f_owner_id
                                 << "- the id is already registered by" << l_entry.owner;
            return;
        }
    }
    m_writers.append({std::move(f_writer), f_owner_id});
}

void LogService::unregisterAll(const QString &f_owner_id)
{
    AKASHI_ASSERT_THREAD_AFFINITY();
    {
        QMutexLocker l_lock(&m_mutex);
        // Hand each retired writer to the worker for disposal instead of
        // dropping it here: the writer may hold a resource affine to the worker
        // thread, so its destructor has to run there. The caller should release
        // its own reference before this call so the worker holds the last one.
        for (auto l_it = m_writers.begin(); l_it != m_writers.end();) {
            if (l_it->owner == f_owner_id) {
                m_pending_disposal.append(std::move(l_it->writer));
                l_it = m_writers.erase(l_it);
            }
            else {
                ++l_it;
            }
        }
    }
    m_condition.wakeOne();
}

void LogService::registerTemplate(const QString &f_type, const QString &f_tmpl)
{
    AKASHI_ASSERT_THREAD_AFFINITY();
    // The plugin door gets the same treatment as the config door: legacy
    // positional templates migrate, unknown placeholders warn.
    QString l_tmpl = f_tmpl;
    if (isPositionalTemplate(l_tmpl)) {
        l_tmpl = migratePositionalTemplate(f_type, l_tmpl);
    }
    warnUnknownPlaceholders(f_type, l_tmpl);
    QMutexLocker l_lock(&m_mutex);
    m_templates.insert(f_type, l_tmpl);
}

void LogService::reloadTemplates()
{
    AKASHI_ASSERT_THREAD_AFFINITY();
    {
        QMutexLocker l_lock(&m_mutex);
        m_templates = defaultTemplates();
    }
    loadTemplates();
}

void LogService::runWriterMaintenance()
{
    AKASHI_ASSERT_THREAD_AFFINITY();
    m_run_maintenance.store(true);
    m_condition.wakeOne();
}

void LogService::stopWorker()
{
    m_stop.store(true);
    m_condition.wakeAll();
    if (m_worker) {
        m_worker->wait();
        delete m_worker;
        m_worker = nullptr;
    }
}

void LogService::workerLoop()
{
    while (!m_stop.load()) {
        QQueue<LogEvent> l_batch;
        QList<WriterEntry> l_writers;
        {
            QMutexLocker l_lock(&m_mutex);
            if (m_queue.isEmpty() && !m_stop.load()) {
                m_condition.wait(&m_mutex);
            }
            l_batch.swap(m_queue);
            l_writers = m_writers;
        }

        for (const auto &l_event : l_batch) {
            for (const auto &l_entry : l_writers) {
                l_entry.writer->write(l_event);
            }
        }
        for (const auto &l_entry : l_writers) {
            l_entry.writer->flush();
        }

        if (m_run_maintenance.exchange(false)) {
            for (const auto &l_entry : l_writers) {
                l_entry.writer->maintenance();
            }
        }

        // Drop retired writers here, on the worker thread, so a thread-affine
        // resource is torn down on the thread that created it.
        QList<std::shared_ptr<ILogWriter>> l_disposal;
        {
            QMutexLocker l_lock(&m_mutex);
            l_disposal.swap(m_pending_disposal);
        }
        l_disposal.clear();
    }

    QQueue<LogEvent> l_remaining;
    QList<WriterEntry> l_writers;
    QList<std::shared_ptr<ILogWriter>> l_final_disposal;
    {
        QMutexLocker l_lock(&m_mutex);
        l_remaining.swap(m_queue);
        l_writers = m_writers;
        l_final_disposal.swap(m_pending_disposal);
    }
    for (const auto &l_event : l_remaining) {
        for (const auto &l_entry : l_writers) {
            l_entry.writer->write(l_event);
        }
    }
    // Anything retired after the last loop iteration still gets disposed here,
    // before the worker thread exits.
    l_final_disposal.clear();
}

QString LogService::applyTemplate(const QString &f_tmpl, const LogEvent &f_event) const
{
    const QString l_timestamp = QDateTime::fromMSecsSinceEpoch(f_event.timestamp)
                                    .toString(QStringLiteral("ddd MMMM d yyyy | hh:mm:ss"));

    QString l_result = f_tmpl;
    l_result.replace(QStringLiteral("{timestamp}"), l_timestamp);
    l_result.replace(QStringLiteral("{area}"), f_event.area);
    l_result.replace(QStringLiteral("{char_name}"), f_event.char_name);
    l_result.replace(QStringLiteral("{ooc_name}"), f_event.ooc_name);
    l_result.replace(QStringLiteral("{ipid}"), f_event.ipid);
    l_result.replace(QStringLiteral("{client_id}"), f_event.client_id);
    l_result.replace(QStringLiteral("{message}"), f_event.message);
    l_result.replace(QStringLiteral("{args}"), f_event.args);
    l_result.replace(QStringLiteral("{moderator}"), f_event.moderator);
    l_result.replace(QStringLiteral("{target_ipid}"), f_event.target_ipid);
    l_result.replace(QStringLiteral("{duration}"), f_event.duration);
    l_result.replace(QStringLiteral("{hwid}"), f_event.hwid);
    l_result.replace(QStringLiteral("{success}"), f_event.success ? QStringLiteral("SUCCESS") : QStringLiteral("FAILED"));
    return l_result;
}

void LogService::loadTemplates()
{
    if (!m_config_store) {
        return;
    }

    QSettings *l_settings = m_config_store->settings(QStringLiteral("text/logtext"));
    if (!l_settings) {
        return;
    }

    for (auto l_it = defaultTemplates().constBegin(); l_it != defaultTemplates().constEnd(); ++l_it) {
        const QString l_value = l_settings->value(QStringLiteral("LogConfiguration/") + l_it.key()).toString();
        if (l_value.isEmpty()) {
            continue;
        }

        if (isPositionalTemplate(l_value)) {
            const QString l_migrated = migratePositionalTemplate(l_it.key(), l_value);
            qCInfo(akashiLog).noquote() << "logtext: migrated positional template for" << l_it.key();
            warnUnknownPlaceholders(l_it.key(), l_migrated);
            QMutexLocker l_lock(&m_mutex);
            m_templates.insert(l_it.key(), l_migrated);
        }
        else {
            warnUnknownPlaceholders(l_it.key(), l_value);
            QMutexLocker l_lock(&m_mutex);
            m_templates.insert(l_it.key(), l_value);
        }
    }
}

QString LogService::migratePositionalTemplate(const QString &f_key, const QString &f_tmpl)
{
    auto l_it = positionalMigration().constFind(f_key);
    if (l_it == positionalMigration().constEnd()) {
        return f_tmpl;
    }

    int l_highest = 0;
    for (int i = 9; i >= 1; --i) {
        if (f_tmpl.contains(QStringLiteral("%") + QString::number(i))) {
            l_highest = i;
            break;
        }
    }

    const QStringList *l_fields = nullptr;
    for (const PositionalLineage &l_lineage : l_it.value()) {
        if (l_highest <= l_lineage.field_count) {
            l_fields = &l_lineage.fields;
            break;
        }
    }
    if (!l_fields) {
        qCWarning(akashiLog).noquote() << "logtext: template" << f_key << "uses %" + QString::number(l_highest)
                                       << "- beyond every known legacy layout, left as written.";
        return f_tmpl;
    }

    QString l_result = f_tmpl;
    for (int i = l_fields->size(); i >= 1; --i) {
        l_result.replace(QStringLiteral("%") + QString::number(i), l_fields->at(i - 1));
    }
    return l_result;
}

void LogService::warnUnknownPlaceholders(const QString &f_key, const QString &f_tmpl)
{
    static const QRegularExpression s_token(QStringLiteral("\\{([a-z_]+)\\}"));
    auto l_matches = s_token.globalMatch(f_tmpl);
    while (l_matches.hasNext()) {
        const QString l_token = l_matches.next().captured(1);
        if (!knownPlaceholders().contains(l_token)) {
            qCWarning(akashiLog).noquote() << "logtext: template" << f_key << "uses unknown placeholder {" + l_token + "}"
                                           << "- it will appear in log lines as written. Known placeholders:"
                                           << knownPlaceholders().join(QStringLiteral(", "));
        }
    }

    // Fork configs write angle-bracket placeholder names; akashi does not
    // migrate foreign dialects, but it names them instead of failing silently.
    static const QRegularExpression s_angle_token(QStringLiteral("<([a-z_]+)>"));
    auto l_angle_matches = s_angle_token.globalMatch(f_tmpl);
    while (l_angle_matches.hasNext()) {
        const QString l_token = l_angle_matches.next().captured(1);
        qCWarning(akashiLog).noquote() << "logtext: template" << f_key << "uses unknown placeholder <" + l_token + ">"
                                       << "- it will appear in log lines as written. Known placeholders:"
                                       << knownPlaceholders().join(QStringLiteral(", "));
    }
}

} // namespace akashi
