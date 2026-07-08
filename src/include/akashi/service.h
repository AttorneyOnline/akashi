#pragma once

#include "akashi_core_export.h"

#include <QString>

namespace akashi {

// A service version, compared against a range like ">=1.2.0,<2.0.0" or "^1.2.0".
struct AKASHI_CORE_EXPORT ServiceVersion
{
    int major = 1;
    int minor = 0;
    int patch = 0;

    // True if this version satisfies the range. An empty range always matches.
    // Grammar: comma-separated comparators (all must hold), each one of
    // >=, >, <=, <, = (or bare) or ^ (same major, at least this version).
    bool satisfies(const QString &f_range) const;

    QString toString() const;
};

// The base class every service derives from, giving it an identity and a version.
class AKASHI_CORE_EXPORT IService
{
  public:
    virtual ~IService() = default;

    // A unique id, like "akashi.music" or "myplugin.stats".
    virtual QString serviceId() const = 0;
    virtual ServiceVersion serviceVersion() const = 0;
};

} // namespace akashi
