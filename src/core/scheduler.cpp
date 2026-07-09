// AI-generated: written by Claude.
#include "akashi/scheduler.h"

#include "akashi/logging_categories.h"

#include <QDebug>
#include <QRegularExpression>
#include <QTimer>

namespace akashi {

// Due jobs postponed by their check retry this much later.
static const int POSTPONE_MSECS = 30 * 60 * 1000;
// Long waits are armed in slices this large and re-checked.
static const qint64 MAX_ARM_MSECS = 6 * 60 * 60 * 1000;

Schedule Schedule::daily(const QTime &f_time)
{
    Schedule l_schedule;
    if (f_time.isValid()) {
        l_schedule.m_kind = Kind::Daily;
        l_schedule.m_time = f_time;
    }
    return l_schedule;
}

Schedule Schedule::weekly(Qt::DayOfWeek f_day, const QTime &f_time)
{
    Schedule l_schedule;
    if (f_time.isValid()) {
        l_schedule.m_kind = Kind::Weekly;
        l_schedule.m_day = f_day;
        l_schedule.m_time = f_time;
    }
    return l_schedule;
}

Schedule Schedule::once(const QDateTime &f_when)
{
    Schedule l_schedule;
    if (f_when.isValid()) {
        l_schedule.m_kind = Kind::Once;
        l_schedule.m_when = f_when;
    }
    return l_schedule;
}

static std::optional<Qt::DayOfWeek> dayOfWeekFromWord(const QString &f_word)
{
    static const QStringList DAY_WORDS = {
        QStringLiteral("monday"), QStringLiteral("tuesday"), QStringLiteral("wednesday"),
        QStringLiteral("thursday"), QStringLiteral("friday"), QStringLiteral("saturday"),
        QStringLiteral("sunday")};
    const int l_day = DAY_WORDS.indexOf(f_word.trimmed().toLower());
    if (l_day < 0) {
        return std::nullopt;
    }
    return static_cast<Qt::DayOfWeek>(l_day + 1);
}

Schedule Schedule::fromDayWord(const QString &f_day, const QTime &f_time)
{
    if (f_day.trimmed().toLower() == QStringLiteral("daily")) {
        return daily(f_time);
    }
    const auto l_day = dayOfWeekFromWord(f_day);
    if (!l_day.has_value()) {
        return {};
    }
    return weekly(*l_day, f_time);
}

std::optional<QDateTime> parseWhen(const QString &f_text, const QDateTime &f_now)
{
    const QString l_text = f_text.trimmed();
    if (l_text.isEmpty()) {
        return std::nullopt;
    }

    // A compound duration: any run of count-unit tokens, added together.
    static const QRegularExpression DURATION_SHAPE(QStringLiteral("^(?:\\d+[ywdhms])+$"),
                                                   QRegularExpression::CaseInsensitiveOption);
    if (DURATION_SHAPE.match(l_text).hasMatch()) {
        static const QRegularExpression DURATION_TOKEN(QStringLiteral("(\\d+)([ywdhms])"),
                                                       QRegularExpression::CaseInsensitiveOption);
        qint64 l_seconds = 0;
        auto l_tokens = DURATION_TOKEN.globalMatch(l_text);
        while (l_tokens.hasNext()) {
            const auto l_token = l_tokens.next();
            const qint64 l_count = l_token.captured(1).toLongLong();
            switch (l_token.captured(2).at(0).toLower().unicode()) {
            case 'y':
                l_seconds += l_count * 365ll * 24 * 60 * 60;
                break;
            case 'w':
                l_seconds += l_count * 7ll * 24 * 60 * 60;
                break;
            case 'd':
                l_seconds += l_count * 24ll * 60 * 60;
                break;
            case 'h':
                l_seconds += l_count * 60ll * 60;
                break;
            case 'm':
                l_seconds += l_count * 60ll;
                break;
            case 's':
                l_seconds += l_count;
                break;
            }
        }
        if (l_seconds <= 0) {
            return std::nullopt;
        }
        return f_now.addSecs(l_seconds);
    }

    // A weekday name means the coming midnight of that day.
    if (const auto l_day = dayOfWeekFromWord(l_text)) {
        QDateTime l_next(f_now.date().addDays(1), QTime(0, 0));
        while (l_next.date().dayOfWeek() != *l_day) {
            l_next = l_next.addDays(1);
        }
        return l_next;
    }

    // A local calendar moment, date first, midnight when no time is given.
    QDateTime l_when = QDateTime::fromString(l_text, QStringLiteral("dd.MM.yyyy HH:mm"));
    if (!l_when.isValid()) {
        const QDate l_date = QDate::fromString(l_text, QStringLiteral("dd.MM.yyyy"));
        if (l_date.isValid()) {
            l_when = QDateTime(l_date, QTime(0, 0));
        }
    }
    if (l_when.isValid() && l_when > f_now) {
        return l_when;
    }
    return std::nullopt;
}

bool Schedule::isValid() const
{
    return m_kind != Kind::Never;
}

bool Schedule::repeats() const
{
    return m_kind == Kind::Daily || m_kind == Kind::Weekly;
}

std::optional<QDateTime> Schedule::nextAfter(const QDateTime &f_now) const
{
    switch (m_kind) {
    case Kind::Never:
        return std::nullopt;
    case Kind::Daily:
    {
        QDateTime l_next(f_now.date(), m_time);
        if (l_next <= f_now) {
            l_next = l_next.addDays(1);
        }
        return l_next;
    }
    case Kind::Weekly:
    {
        QDateTime l_next(f_now.date(), m_time);
        while (l_next.date().dayOfWeek() != m_day || l_next <= f_now) {
            l_next = l_next.addDays(1);
        }
        return l_next;
    }
    case Kind::Once:
        // A one-shot in the past is due immediately, so a missed job
        // still runs - a sanction that should have lifted, lifts.
        return m_when > f_now ? m_when : f_now;
    }
    return std::nullopt;
}

Scheduler::Scheduler(QObject *parent) :
    QObject(parent)
{}

QString Scheduler::serviceId() const
{
    return QStringLiteral("akashi.scheduler");
}

ServiceVersion Scheduler::serviceVersion() const
{
    return {1, 0, 0};
}

bool Scheduler::schedule(const QString &f_id, const Schedule &f_schedule, std::function<void()> f_action,
                         const QString &f_owner_id, const std::function<bool()> &f_postpone)
{
    if (f_id.isEmpty() || !f_schedule.isValid() || !f_action) {
        return false;
    }
    cancel(f_id);

    Job l_job;
    l_job.id = f_id;
    l_job.schedule = f_schedule;
    l_job.action = std::move(f_action);
    l_job.owner_id = f_owner_id;
    l_job.postpone = f_postpone;
    l_job.next_due = *f_schedule.nextAfter(QDateTime::currentDateTime());
    m_jobs.append(std::move(l_job));

    qCInfo(akashiServer).noquote() << QStringLiteral("Scheduled \"%1\" - next run %2")
                                          .arg(f_id, m_jobs.last().next_due.toString(QStringLiteral("yyyy-MM-dd hh:mm")));
    rearm();
    return true;
}

void Scheduler::cancel(const QString &f_id)
{
    m_jobs.removeIf([&f_id](const Job &f_job) { return f_job.id == f_id; });
    rearm();
}

void Scheduler::cancelAll(const QString &f_owner_id)
{
    m_jobs.removeIf([&f_owner_id](const Job &f_job) { return f_job.owner_id == f_owner_id; });
    rearm();
}

std::optional<QDateTime> Scheduler::nextRunAt(const QString &f_id) const
{
    for (const Job &l_job : m_jobs) {
        if (l_job.id == f_id) {
            return l_job.next_due;
        }
    }
    return std::nullopt;
}

bool Scheduler::runNow(const QString &f_id)
{
    for (int i = 0; i < m_jobs.size(); i++) {
        if (m_jobs[i].id != f_id) {
            continue;
        }
        const auto l_action = m_jobs[i].action;
        if (!m_jobs[i].schedule.repeats()) {
            m_jobs.removeAt(i);
            rearm();
        }
        l_action();
        return true;
    }
    return false;
}

void Scheduler::onTick()
{
    // The job list settles before any action runs, so an action may
    // schedule or cancel jobs without upsetting this pass.
    const QDateTime l_now = QDateTime::currentDateTime();
    QList<std::function<void()>> l_due;
    for (int i = m_jobs.size() - 1; i >= 0; i--) {
        if (m_jobs[i].next_due > l_now) {
            continue;
        }
        if (m_jobs[i].postpone && m_jobs[i].postpone()) {
            qCInfo(akashiServer).noquote() << QStringLiteral("Postponed \"%1\" by half an hour.").arg(m_jobs[i].id);
            m_jobs[i].next_due = l_now.addMSecs(POSTPONE_MSECS);
            continue;
        }
        l_due.append(m_jobs[i].action);
        if (m_jobs[i].schedule.repeats()) {
            m_jobs[i].next_due = *m_jobs[i].schedule.nextAfter(l_now);
        }
        else {
            m_jobs.removeAt(i);
        }
    }
    for (const auto &l_action : std::as_const(l_due)) {
        l_action();
    }
    rearm();
}

void Scheduler::rearm()
{
    if (m_jobs.isEmpty()) {
        if (m_timer) {
            m_timer->stop();
        }
        return;
    }
    if (!m_timer) {
        m_timer = new QTimer(this);
        m_timer->setSingleShot(true);
        connect(m_timer, &QTimer::timeout, this, &Scheduler::onTick);
    }

    const QDateTime l_now = QDateTime::currentDateTime();
    QDateTime l_soonest = m_jobs.first().next_due;
    for (const Job &l_job : std::as_const(m_jobs)) {
        if (l_job.next_due < l_soonest) {
            l_soonest = l_job.next_due;
        }
    }
    const qint64 l_wait = qBound(qint64(0), l_now.msecsTo(l_soonest), MAX_ARM_MSECS);
    m_timer->start(int(l_wait));
}

} // namespace akashi
