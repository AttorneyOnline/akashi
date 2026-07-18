#include "core/auth_throttle.h"

namespace akashi {

AuthThrottle::AuthThrottle(int f_max_attempts, int f_lockout_seconds) :
    m_max_attempts(f_max_attempts),
    m_lockout_seconds(f_lockout_seconds)
{}

void AuthThrottle::setLimits(int f_max_attempts, int f_lockout_seconds)
{
    m_max_attempts = f_max_attempts;
    m_lockout_seconds = f_lockout_seconds;
}

bool AuthThrottle::isLockedOut(const QString &f_ipid) const
{
    auto l_it = m_attempts.constFind(f_ipid);
    if (l_it == m_attempts.constEnd())
        return false;
    return l_it->locked_out && !l_it->lockout_until.hasExpired();
}

int AuthThrottle::remainingLockoutSeconds(const QString &f_ipid) const
{
    auto l_it = m_attempts.constFind(f_ipid);
    if (l_it == m_attempts.constEnd())
        return 0;
    if (!l_it->locked_out || l_it->lockout_until.hasExpired())
        return 0;
    qint64 l_remaining = l_it->lockout_until.remainingTime();
    return l_remaining > 0 ? static_cast<int>((l_remaining + 999) / 1000) : 0;
}

void AuthThrottle::recordFailure(const QString &f_ipid)
{
    prune();
    AttemptRecord &l_record = m_attempts[f_ipid];
    if (l_record.locked_out && l_record.lockout_until.hasExpired()) {
        l_record.failed_count = 0;
        l_record.locked_out = false;
    }
    l_record.failed_count++;
    l_record.stale_after = QDeadlineTimer(m_lockout_seconds * 1000);
    if (l_record.failed_count >= m_max_attempts) {
        l_record.lockout_until = QDeadlineTimer(m_lockout_seconds * 1000);
        l_record.locked_out = true;
        l_record.stale_after = l_record.lockout_until;
    }
}

void AuthThrottle::recordSuccess(const QString &f_ipid)
{
    m_attempts.remove(f_ipid);
}

void AuthThrottle::reset(const QString &f_ipid)
{
    m_attempts.remove(f_ipid);
}

void AuthThrottle::prune()
{
    // Stale records influence nothing, so drop them - otherwise the map
    // grows by one permanent entry per ipid that ever failed a login.
    m_attempts.removeIf([](const std::pair<const QString &, AttemptRecord &> &f_entry) {
        return f_entry.second.stale_after.hasExpired();
    });
}

} // namespace akashi
