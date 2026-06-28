#pragma once

#include "akashi/service.h"
#include "akashi_core_export.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>
#include <optional>

class QPluginLoader;

namespace akashi {

class IPlugin;
class ServiceRegistry;

struct PluginInfo
{
    QString id;
    ServiceVersion version;
    QStringList dependencies;
    QStringList services;
    QString file_path;

    // Set for script plugins: the language runtime ("lua", "python") and
    // the entry script the matching host runs. Empty for native plugins.
    QString runtime;
    QString entry_path;

    enum class State { Discovered, Loaded, Initialized, Started, Failed };
    State state = State::Discovered;
};

class AKASHI_CORE_EXPORT PluginManager : public QObject, public IService
{
    Q_OBJECT

  public:
    explicit PluginManager(ServiceRegistry *f_services, const QString &f_plugin_dir,
                           QObject *parent = nullptr);
    ~PluginManager() override;

    QString serviceId() const override;
    ServiceVersion serviceVersion() const override;

    bool startPlugins(const QStringList &f_allowlist = {});
    void shutdownAll();

    bool loadPlugin(const QString &f_id);
    bool unloadPlugin(const QString &f_id, bool f_cascade = false);
    bool reloadPlugin(const QString &f_id);

    QList<PluginInfo> plugins() const;
    std::optional<PluginInfo> pluginInfo(const QString &f_id) const;

    // Reads a script plugin's declaration header: the JSON object after the
    // "akashi-plugin" marker near the top of the file. Every field has a
    // default - the id falls back to the file name, the version to 1.0.0,
    // the runtime to the file extension - and the matching host plugin is
    // added to the dependencies. Returns nothing when the marker is absent
    // or the object does not parse.
    static std::optional<PluginInfo> parseScriptHeader(const QString &f_file_path);

  Q_SIGNALS:
    /**
     * @brief Fires while a plugin's registrations are being removed and
     * before its library unloads. Anything holding functions built by the
     * plugin (like applied rules) must drop them now.
     */
    void pluginAboutToUnload(const QString &f_plugin_id);

  private:
    struct PluginEntry
    {
        PluginInfo info;
        QPluginLoader *loader = nullptr;
        IPlugin *instance = nullptr;

        bool isScript() const { return !info.runtime.isEmpty(); }
        bool isActive() const
        {
            return info.state == PluginInfo::State::Loaded ||
                   info.state == PluginInfo::State::Initialized ||
                   info.state == PluginInfo::State::Started;
        }
    };

    bool discover(const QStringList &f_allowlist);
    void discoverScriptPlugin(const QString &f_dir_path, const QStringList &f_allowlist);
    bool loadScriptEntry(PluginEntry &f_entry);
    void unloadScriptEntry(PluginEntry &f_entry);
    QStringList topologicalSort() const;
    bool validateServices(const PluginEntry &f_entry) const;
    void cleanupPlugin(const QString &f_id);
    QStringList dependentsOf(const QString &f_id) const;

    ServiceRegistry *m_services;
    QString m_plugin_dir;
    QHash<QString, PluginEntry> m_plugins;
    QList<QString> m_load_order;
};

} // namespace akashi

