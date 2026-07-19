#pragma once

#include "akashi/config_entry.h"
#include "akashi/service.h"
#include "akashi_core_export.h"

#include <QHash>
#include <QObject>
#include <QSettings>

#include <utility>

namespace akashi {

class SettingNotifier;

// Owns the configuration files. Plugins get their own file through declarePlugin().
class AKASHI_CORE_EXPORT ConfigStore : public QObject, public IService
{
    Q_OBJECT

  public:
    explicit ConfigStore(const QString &f_root = resolveRootPath(), QObject *parent = nullptr);

    QString serviceId() const override;
    ServiceVersion serviceVersion() const override;

    // The folder all config files live in.
    QString rootPath() const;

    // The path of a file inside the config folder.
    QString filePath(const QString &f_file_name) const;

    // Declares the settings of a config file, for example "config" for config.json.
    // Loads the file, converts a leftover INI file and checks every value.
    // Returns false if a declared setting has an unusable value.
    bool declare(const QString &f_name, const QList<ConfigEntry> &f_entries);

    // Declares the settings of a plugin. The format name selects the file
    // extension and parser (default "json"). Other formats like "toml" must
    // be registered first by an integration plugin.
    bool declarePlugin(const QString &f_plugin_id, const QList<ConfigEntry> &f_entries,
                       const QString &f_format = QStringLiteral("json"));

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

    // Registers a named config format backed by a QSettings::Format handle.
    // JSON is pre-registered as "json". An integration plugin (e.g. toml-support)
    // calls this to make its format available to other plugins.
    void registerFormat(const QString &f_name, QSettings::Format f_format,
                        const QString &f_owner = {});

    void unregisterFormat(const QString &f_name);

    // Records that a file should use the given format. Called internally by
    // declarePlugin; exposed for plugins that use the Settings wrapper class.
    void registerFileFormat(const QString &f_name, const QString &f_format);

    // Raw settings for files with free-form keys, like areas.json. Owned by the store.
    // A leftover <name>.ini is converted to the file's format once if the target does not exist yet.
    QSettings *settings(const QString &f_name);

    // Copies the settings of an outdated file into its current home, once,
    // skipped when the target already exists; the source stays behind as a
    // backup. Without a new name this is the INI-to-current-format
    // conversion every settings() call runs. With one, the file also
    // changes its base name - the newest home of the old name is the
    // source, so a converted file wins over the INI it came from.
    void migrateConfigFile(const QString &f_name, const QString &f_extension, const QString &f_new_name = QString());

    // Markdown reference for a declared file, generated from the declarations.
    QString documentation(const QString &f_name) const;

    // Reloads every open file from disk and rechecks the declared values.
    void reload();

    // Returns a notifier for the given (file, key) pair, creating one if needed.
    // The notifier is owned by the store; its changed() signal fires on reload
    // when the value on disk differs from the previous value.
    SettingNotifier *notifier(const QString &f_name, const QString &f_key);

    // Picks the config folder from --config-root, AKASHI_CONFIG_ROOT or the default config/.
    static QString resolveRootPath();

  Q_SIGNALS:
    void configReloaded();
    // Emitted on reload for every declared setting whose value changed.
    void valueChanged(const QString &f_name, const QString &f_key, const QVariant &f_old, const QVariant &f_new);

  private:
    bool loadDeclaredValues(const QString &f_name);
    const ConfigEntry *findEntry(const QString &f_name, const QString &f_key) const;
    QString formatExtension(const QString &f_name) const;

    QString m_root;
    QHash<QString, QSettings *> m_open_settings;
    QHash<QString, QList<ConfigEntry>> m_entries;
    QHash<QString, QHash<QString, QVariant>> m_values;
    QHash<std::pair<QString, QString>, SettingNotifier *> m_notifiers;

    struct FormatEntry
    {
        QSettings::Format format;
        QString owner;
    };
    QHash<QString, FormatEntry> m_formats;
    QHash<QString, QString> m_file_formats;
};

} // namespace akashi
