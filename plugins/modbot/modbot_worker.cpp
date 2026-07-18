#include "modbot_worker.h"

#include "akashi/logging_categories.h"

#include <QSqlError>
#include <QSqlQuery>

namespace akashi::modbot {

BoundedEventQueue::BoundedEventQueue(int f_capacity) :
    m_capacity(f_capacity > 0 ? f_capacity : 1)
{}

bool BoundedEventQueue::enqueue(const Event &f_event)
{
    QMutexLocker l_lock(&m_mutex);
    if (m_closed) {
        return true;
    }
    bool l_kept_everything = true;
    if (m_events.size() >= m_capacity) {
        m_events.removeFirst();
        m_dropped++;
        l_kept_everything = false;
    }
    m_events.append(f_event);
    m_wake.wakeOne();
    return l_kept_everything;
}

QList<Event> BoundedEventQueue::waitAndDrain()
{
    QMutexLocker l_lock(&m_mutex);
    while (m_events.isEmpty() && !m_closed) {
        m_wake.wait(&m_mutex);
    }
    QList<Event> l_batch;
    l_batch.swap(m_events);
    return l_batch;
}

void BoundedEventQueue::close()
{
    QMutexLocker l_lock(&m_mutex);
    m_closed = true;
    m_wake.wakeAll();
}

int BoundedEventQueue::droppedCount() const
{
    QMutexLocker l_lock(&m_mutex);
    return m_dropped;
}

IncidentStore::~IncidentStore()
{
    close();
}

bool IncidentStore::open(const QString &f_db_path, const QString &f_connection_name)
{
    m_connection_name = f_connection_name;
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connection_name);
    m_db.setDatabaseName(f_db_path);
    if (!m_db.open()) {
        qCWarning(akashiPlugins) << "modbot: cannot open the incident store:" << m_db.lastError().text();
        return false;
    }
    QSqlQuery l_query(m_db);
    if (!l_query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS incidents ("
            "id INTEGER PRIMARY KEY, "
            "epoch INTEGER NOT NULL, "
            "ipid TEXT NOT NULL, "
            "kind TEXT NOT NULL, "
            "reason TEXT)"))) {
        qCWarning(akashiPlugins) << "modbot: cannot create the incident table:" << l_query.lastError().text();
        return false;
    }
    l_query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_incidents_ipid ON incidents(ipid, epoch)"));
    return true;
}

void IncidentStore::close()
{
    if (m_db.isValid()) {
        m_db.close();
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connection_name);
    }
}

int IncidentStore::incidentCount(const QString &f_ipid) const
{
    if (!m_db.isOpen()) {
        return 0;
    }
    QSqlQuery l_query(m_db);
    l_query.prepare(QStringLiteral("SELECT COUNT(*) FROM incidents WHERE ipid = ?"));
    l_query.addBindValue(f_ipid);
    if (!l_query.exec() || !l_query.first()) {
        return 0;
    }
    return l_query.value(0).toInt();
}

void IncidentStore::record(qint64 f_epoch, const QString &f_ipid, const QString &f_kind, const QString &f_reason)
{
    if (!m_db.isOpen()) {
        return;
    }
    QSqlQuery l_query(m_db);
    l_query.prepare(QStringLiteral("INSERT INTO incidents(epoch, ipid, kind, reason) VALUES(?, ?, ?, ?)"));
    l_query.addBindValue(f_epoch);
    l_query.addBindValue(f_ipid);
    l_query.addBindValue(f_kind);
    l_query.addBindValue(f_reason);
    if (!l_query.exec()) {
        qCWarning(akashiPlugins) << "modbot: incident not recorded:" << l_query.lastError().text();
    }
}

static QString verdictKindName(Verdict::Action f_action)
{
    switch (f_action) {
    case Verdict::Action::Warn:
        return QStringLiteral("warn");
    case Verdict::Action::Mute:
        return QStringLiteral("mute");
    case Verdict::Action::Kick:
        return QStringLiteral("kick");
    }
    return QStringLiteral("warn");
}

Worker::Worker(const Config &f_config, const QString &f_db_path, QObject *parent) :
    QThread(parent),
    m_config(f_config),
    m_db_path(f_db_path),
    m_queue(1024)
{}

void Worker::stop()
{
    m_queue.close();
    wait();
}

void Worker::run()
{
    // The store lives and dies on this thread; Qt SQL connections must
    // never cross threads, so the main side only ever passed the path.
    IncidentStore l_store;
    l_store.open(m_db_path, QStringLiteral("akashi_modbot_worker"));
    Rules l_rules(m_config);

    while (true) {
        const QList<Event> l_batch = m_queue.waitAndDrain();
        if (l_batch.isEmpty()) {
            break;
        }
        for (const Event &l_event : l_batch) {
            // Human moderation feeds the history the ladder climbs on.
            if (l_event.kind == Event::Kind::BanIssued) {
                l_store.record(l_event.epoch, l_event.ipid, QStringLiteral("ban"), l_event.message);
                continue;
            }
            if (l_event.kind == Event::Kind::KickIssued) {
                l_store.record(l_event.epoch, l_event.ipid, QStringLiteral("kick"), l_event.message);
                continue;
            }

            const int l_prior = l_store.incidentCount(l_event.ipid);
            const QList<Verdict> l_verdicts = l_rules.analyze(l_event, l_prior);
            for (const Verdict &l_verdict : l_verdicts) {
                l_store.record(l_event.epoch, l_verdict.ipid, verdictKindName(l_verdict.action), l_verdict.reason);
                Q_EMIT verdictReady(l_verdict);
            }
        }
    }
    l_store.close();
}

} // namespace akashi::modbot
