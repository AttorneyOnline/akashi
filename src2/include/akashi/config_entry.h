#ifndef AKASHI_CONFIG_ENTRY_H
#define AKASHI_CONFIG_ENTRY_H

#include "akashi_core_export.h"

#include <QString>
#include <QVariant>

#include <functional>

namespace akashi {

// One declared setting: key, typed default, description and an optional value check.
class AKASHI_CORE_EXPORT ConfigEntry
{
  public:
    using Check = std::function<bool(const QVariant &)>;

    ConfigEntry(const QString &f_key, const QVariant &f_default, const QString &f_description, Check f_check = {});

    QString key() const;
    QVariant defaultValue() const;
    // The type of a setting is the type of its default value.
    int typeId() const;
    QString description() const;
    bool checkValue(const QVariant &f_value) const;

  private:
    QString m_key;
    QVariant m_default;
    QString m_description;
    Check m_check;
};

// The value must be a number between min and max.
AKASHI_CORE_EXPORT ConfigEntry::Check inRange(double f_min, double f_max);
// The value must be a number of at least min.
AKASHI_CORE_EXPORT ConfigEntry::Check atLeast(double f_min);
// The value must be one of the given words, compared without case.
AKASHI_CORE_EXPORT ConfigEntry::Check oneOf(const QStringList &f_words);

} // namespace akashi

#endif // AKASHI_CONFIG_ENTRY_H
