#include "akashi/config_entry.h"

namespace akashi {

ConfigEntry::ConfigEntry(const QString &f_key, const QVariant &f_default, const QString &f_description, Check f_check) :
    m_key(f_key),
    m_default(f_default),
    m_description(f_description),
    m_check(f_check)
{}

QString ConfigEntry::key() const
{
    return m_key;
}

QVariant ConfigEntry::defaultValue() const
{
    return m_default;
}

int ConfigEntry::typeId() const
{
    return m_default.typeId();
}

QString ConfigEntry::description() const
{
    return m_description;
}

bool ConfigEntry::checkValue(const QVariant &f_value) const
{
    return m_check ? m_check(f_value) : true;
}

ConfigEntry::Check inRange(double f_min, double f_max)
{
    return [f_min, f_max](const QVariant &f_value) {
        const double l_number = f_value.toDouble();
        return l_number >= f_min && l_number <= f_max;
    };
}

ConfigEntry::Check atLeast(double f_min)
{
    return [f_min](const QVariant &f_value) {
        return f_value.toDouble() >= f_min;
    };
}

ConfigEntry::Check oneOf(const QStringList &f_words)
{
    return [f_words](const QVariant &f_value) {
        return f_words.contains(f_value.toString(), Qt::CaseInsensitive);
    };
}

} // namespace akashi
