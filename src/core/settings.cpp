#include "akashi/settings.h"

namespace akashi {

Settings::Settings(ConfigStore *f_store, const QString &f_file) :
    m_store(f_store),
    m_file(f_file)
{}

bool Settings::declare()
{
    return m_store->declare(m_file, m_entries);
}

ConfigStore *Settings::store() const
{
    return m_store;
}

QString Settings::file() const
{
    return m_file;
}

void Settings::registerEntry(const ConfigEntry &f_entry)
{
    m_entries.append(f_entry);
}

} // namespace akashi
