#ifndef AKASHI_CONFIG_STORE_H
#define AKASHI_CONFIG_STORE_H

#include "akashi/config_entry.h"
#include "akashi_core_export.h"

#include <QHash>
#include <QObject>
#include <QSettings>

namespace akashi {

// Owns the configuration files. Plugins get their own file through declarePlugin().
class AKASHI_CORE_EXPORT ConfigStore : public QObject
{
    Q_OBJECT

  public:
    explicit ConfigStore(const QString &f_root = resolveRootPath(), QObject *parent = nullptr);

    // The folder all config files live in.
    QString rootPath() const;

    // The path of a file inside the config folder.
    QString filePath(const QString &f_file_name) const;

    // Declares the settings of a config file, for example "config" for config.json.
    // Loads the file, converts a leftover INI file and checks every value.
    // Returns false if a declared setting has an unusable value.
    bool declare(const QString &f_name, const QList<ConfigEntry> &f_entries);

    // Declares the settings of a plugin, stored as plugins/<id>.json.
    bool declarePlugin(const QString &f_plugin_id, const QList<ConfigEntry> &f_entries);

    // The checked value of a declared setting, with the default filled in.
    QVariant value(const QString &f_name, const QString &f_key) const;

    // Writes a declared setting to the file and keeps the checked value in sync.
    void setValue(const QString &f_name, const QString &f_key, const QVariant &f_value);

    // Typed access to a declared setting.
    template <typename T>
    T get(const QString &f_name, const QString &f_key) const
    {
        return value(f_name, f_key).value<T>();
    }

    // Raw settings for files with free-form keys, like areas.json. Owned by the store.
    // A leftover <name>.ini is converted to <name>.json once, if the JSON file does not exist yet.
    QSettings *settings(const QString &f_name);

    // Markdown reference for a declared file, generated from the declarations.
    QString documentation(const QString &f_name) const;

    // Reloads every open file from disk and rechecks the declared values.
    void reload();

    // Picks the config folder from --config-root, AKASHI_CONFIG_ROOT or the default config/.
    static QString resolveRootPath();

  signals:
    void configReloaded();
    // Emitted on reload for every declared setting whose value changed.
    void valueChanged(const QString &f_name, const QString &f_key, const QVariant &f_old, const QVariant &f_new);

  private:
    void migrateIniFile(const QString &f_name);
    bool loadDeclaredValues(const QString &f_name);
    const ConfigEntry *findEntry(const QString &f_name, const QString &f_key) const;

    QString m_root;
    QHash<QString, QSettings *> m_open_settings;
    QHash<QString, QList<ConfigEntry>> m_entries;
    QHash<QString, QHash<QString, QVariant>> m_values;
};

} // namespace akashi

#endif // AKASHI_CONFIG_STORE_H
