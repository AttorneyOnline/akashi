// AI-generated: written by Claude.
#pragma once

#include "akashi/service.h"
#include "akashi_core_export.h"

#include <QDateTime>
#include <QObject>
#include <QString>

#include <functional>
#include <optional>

class QTimer;

namespace akashi {

// When a job fires and how it repeats. A default-constructed schedule
// never fires.
class AKASHI_CORE_EXPORT Schedule
{
  public:
    Schedule() = default;

    static Schedule daily(const QTime &f_time);
    static Schedule weekly(Qt::DayOfWeek f_day, const QTime &f_time);
    static Schedule once(const QDateTime &f_when);
    // Builds a schedule from configuration words: "daily" or a weekday
    // name picks the rhythm; an invalid time or unknown word never fires.
    static Schedule fromDayWord(const QString &f_day, const QTime &f_time);

    bool isValid() const;
    bool repeats() const;

    // The first fire time after the given moment; empty for a schedule
    // that never fires. A one-shot in the past is due immediately.
    std::optional<QDateTime> nextAfter(const QDateTime &f_now) const;

  private:
    enum class Kind
    {
        Never,
        Daily,
        Weekly,
        Once,
    };

    Kind m_kind = Kind::Never;
    QTime m_time;
    Qt::DayOfWeek m_day = Qt::Monday;
    QDateTime m_when;
};

// Reads a future moment from operator input, relative to f_now: a
// compound duration like "1w2d3h30m" (y, w, d, h, m, s in any order), a
// weekday name meaning the coming midnight of that day, or a local
// calendar moment like "01.01.2028 18:00" (the time may be left off for
// midnight). Empty for anything unreadable or not in the future.
AKASHI_CORE_EXPORT std::optional<QDateTime> parseWhen(const QString &f_text, const QDateTime &f_now);

// Named timed jobs on the main thread: daily and weekly operator jobs,
// and one-shot jobs for things like a sanction that lifts itself. A
// plugin's jobs leave with it through cancelAll.
class AKASHI_CORE_EXPORT Scheduler : public QObject, public IService
{
    Q_OBJECT

  public:
    explicit Scheduler(QObject *parent = nullptr);

    QString serviceId() const override;
    ServiceVersion serviceVersion() const override;

    // Registers a job under an id, replacing a job with the same id. The
    // postpone check may delay a due run; it is retried half an hour
    // later. Returns false when the id, schedule or action is unusable.
    bool schedule(const QString &f_id, const Schedule &f_schedule, std::function<void()> f_action,
                  const QString &f_owner_id = {}, const std::function<bool()> &f_postpone = {});

    void cancel(const QString &f_id);
    void cancelAll(const QString &f_owner_id);

    // When the job next fires; empty for an unknown id.
    std::optional<QDateTime> nextRunAt(const QString &f_id) const;

    // Runs the job immediately. A repeating job keeps its rhythm; a
    // one-shot is spent by running.
    bool runNow(const QString &f_id);

  private:
    struct Job
    {
        QString id;
        Schedule schedule;
        std::function<void()> action;
        QString owner_id;
        std::function<bool()> postpone;
        QDateTime next_due;
    };

    void onTick();
    void rearm();

    QList<Job> m_jobs;
    QTimer *m_timer = nullptr;
};

} // namespace akashi
