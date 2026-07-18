#pragma once

#include "modbot_rules.h"

#include <QMutex>
#include <QSqlDatabase>
#include <QThread>
#include <QWaitCondition>

namespace akashi::modbot {

// The hand-off between the main loop and the analysis thread. Bounded:
// enqueue never blocks, and when the queue is full the oldest entry is
// dropped - overload costs the bot context, never costs players latency.
class BoundedEventQueue
{
  public:
    explicit BoundedEventQueue(int f_capacity);

    // Returns false when the queue had to drop its oldest entry to fit.
    bool enqueue(const Event &f_event);

    // Blocks until events arrive or the queue closes, then hands over
    // everything at once. An empty result means the queue is closed.
    QList<Event> waitAndDrain();

    void close();
    int droppedCount() const;

  private:
    mutable QMutex m_mutex;
    QWaitCondition m_wake;
    QList<Event> m_events;
    int m_capacity;
    int m_dropped = 0;
    bool m_closed = false;
};

// The bot's own longitudinal incident log, in its per-plugin database.
// Lives entirely on the analysis thread: one connection, one owner.
class IncidentStore
{
  public:
    ~IncidentStore();

    bool open(const QString &f_db_path, const QString &f_connection_name);
    void close();

    int incidentCount(const QString &f_ipid) const;
    void record(qint64 f_epoch, const QString &f_ipid, const QString &f_kind, const QString &f_reason);

  private:
    QSqlDatabase m_db;
    QString m_connection_name;
};

// The analysis thread: drains the queue, weighs each snapshot against the
// rules and the actor's history, and hands verdicts back to the main
// thread as queued signals. It never touches a core API - only its own
// snapshots and its own database.
class Worker : public QThread
{
    Q_OBJECT

  public:
    Worker(const Config &f_config, const QString &f_db_path, QObject *parent = nullptr);

    BoundedEventQueue &queue() { return m_queue; }

    // Closes the queue and joins the thread.
    void stop();

  Q_SIGNALS:
    void verdictReady(const akashi::modbot::Verdict &f_verdict);

  protected:
    void run() override;

  private:
    Config m_config;
    QString m_db_path;
    BoundedEventQueue m_queue;
};

} // namespace akashi::modbot
