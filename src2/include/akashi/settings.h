#pragma once

#include "akashi/config_store.h"
#include "akashi/setting_notifier.h"

namespace akashi {

// Base class for a settings file. Setting members register themselves with it.
class AKASHI_CORE_EXPORT Settings
{
  public:
    Settings(ConfigStore *f_store, const QString &f_file);

    // Checks the file against the registered settings. Returns false on an unusable value.
    bool declare();

    ConfigStore *store() const;
    QString file() const;
    void registerEntry(const ConfigEntry &f_entry);

  private:
    ConfigStore *m_store;
    QString m_file;
    QList<ConfigEntry> m_entries;
};

// One typed setting, declared once as a class member and read by calling it.
template <typename T>
class Setting
{
  public:
    Setting(Settings *f_owner, const QString &f_key, const T &f_default, const QString &f_description, ConfigEntry::Check f_check = {}) :
        m_owner(f_owner),
        m_key(f_key)
    {
        f_owner->registerEntry(ConfigEntry(f_key, QVariant::fromValue(f_default), f_description, f_check));
    }

    // The checked value from the config file.
    T operator()() const
    {
        return m_owner->store()->get<T>(m_owner->file(), m_key);
    }

    void set(const T &f_value)
    {
        m_owner->store()->setValue(m_owner->file(), m_key, QVariant::fromValue(f_value));
    }

    QString key() const
    {
        return m_key;
    }

    // Returns a notifier whose changed() signal fires when this setting
    // is modified on disk and reloaded. Owned by the ConfigStore.
    SettingNotifier *notifier()
    {
        return m_owner->store()->notifier(m_owner->file(), m_key);
    }

  private:
    Settings *m_owner;
    QString m_key;
};

} // namespace akashi

