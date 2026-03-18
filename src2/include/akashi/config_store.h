#ifndef AKASHI_CONFIG_STORE_H
#define AKASHI_CONFIG_STORE_H

#include "akashi_core_export.h"

#include <QHash>
#include <QObject>
#include <QSettings>

namespace akashi {

// Owns the configuration files. Plugins get their own file through pluginSettings().
class AKASHI_CORE_EXPORT ConfigStore : public QObject
{
    Q_OBJECT

  public:
    explicit ConfigStore(const QString &f_root = resolveRootPath(), QObject *parent = nullptr);

    // The folder all config files live in.
    QString rootPath() const;

    // The path of a file inside the config folder.
    QString filePath(const QString &f_file_name) const;

    // Settings for a config file, for example "config" for config.json. Owned by the store.
    // A leftover <name>.ini is converted to <name>.json once, if the JSON file does not exist yet.
    QSettings *settings(const QString &f_name);

    // Settings for a plugin, stored as plugins/<id>.json inside the config folder.
    QSettings *pluginSettings(const QString &f_plugin_id);

    // Reloads every open settings file from disk.
    void reload();

    // Picks the config folder from --config-root, AKASHI_CONFIG_ROOT or the default config/.
    static QString resolveRootPath();

  signals:
    void configReloaded();

  private:
    void migrateIniFile(const QString &f_name);

    QString m_root;
    QHash<QString, QSettings *> m_open_settings;
};

} // namespace akashi

#endif // AKASHI_CONFIG_STORE_H
