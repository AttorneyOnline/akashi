#pragma once

#include "akashi_core_export.h"

#include <QDeadlineTimer>
#include <QHash>
#include <QString>

namespace akashi {

class AKASHI_CORE_EXPORT AuthThrottle
{
  public:
    AuthThrottle(int f_max_attempts = 5, int f_lockout_seconds = 60);

    void setLimits(int f_max_attempts, int f_lockout_seconds);

    bool isLockedOut(const QString &f_ipid) const;

    int remainingLockoutSeconds(const QString &f_ipid) const;

    void recordFailure(const QString &f_ipid);

    void recordSuccess(const QString &f_ipid);

    void reset(const QString &f_ipid);

  private:
    void prune();

    struct AttemptRecord
    {
        int failed_count = 0;
        bool locked_out = false;
        QDeadlineTimer lockout_until;
        // When the record stops mattering: failures decay after a quiet
        // lockout window, an ended lockout resets on its next failure anyway.
        QDeadlineTimer stale_after;
    };

    QHash<QString, AttemptRecord> m_attempts;
    int m_max_attempts;
    int m_lockout_seconds;
};

} // namespace akashi
