#include "akashi/service.h"

#include <QStringList>

namespace akashi {

// Compares this version to the given one: -1 if lower, 0 if equal, 1 if higher.
static int compareTo(const ServiceVersion &f_version, int f_major, int f_minor, int f_patch)
{
    if (f_version.major != f_major) {
        return f_version.major < f_major ? -1 : 1;
    }
    if (f_version.minor != f_minor) {
        return f_version.minor < f_minor ? -1 : 1;
    }
    if (f_version.patch != f_patch) {
        return f_version.patch < f_patch ? -1 : 1;
    }
    return 0;
}

// Checks this version against a single comparator like ">=1.2.0" or "^1.0.0".
static bool satisfiesOne(const ServiceVersion &f_version, const QString &f_comparator)
{
    QString l_operator = "=";
    QString l_value = f_comparator;
    for (const QString &l_candidate : {QStringLiteral(">="), QStringLiteral("<="), QStringLiteral(">"), QStringLiteral("<"), QStringLiteral("="), QStringLiteral("^")}) {
        if (f_comparator.startsWith(l_candidate)) {
            l_operator = l_candidate;
            l_value = f_comparator.mid(l_candidate.size());
            break;
        }
    }

    const QStringList l_parts = l_value.trimmed().split('.');
    const int l_major = l_parts.value(0).toInt();
    const int l_minor = l_parts.value(1).toInt();
    const int l_patch = l_parts.value(2).toInt();
    const int l_comparison = compareTo(f_version, l_major, l_minor, l_patch);

    if (l_operator == ">=") {
        return l_comparison >= 0;
    }
    if (l_operator == "<=") {
        return l_comparison <= 0;
    }
    if (l_operator == ">") {
        return l_comparison > 0;
    }
    if (l_operator == "<") {
        return l_comparison < 0;
    }
    if (l_operator == "^") {
        // The same major version, at least the given version.
        return l_comparison >= 0 && f_version.major == l_major;
    }
    return l_comparison == 0;
}

bool ServiceVersion::satisfies(const QString &f_range) const
{
    if (f_range.trimmed().isEmpty()) {
        return true;
    }
    const QStringList l_comparators = f_range.split(',', Qt::SkipEmptyParts);
    for (const QString &l_comparator : l_comparators) {
        if (!satisfiesOne(*this, l_comparator.trimmed())) {
            return false;
        }
    }
    return true;
}

QString ServiceVersion::toString() const
{
    return QString("%1.%2.%3").arg(major).arg(minor).arg(patch);
}

} // namespace akashi
