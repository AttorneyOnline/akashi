#include "core/plugin_manager.h"

#include "akashi/plugin.h"
#include "akashi/script_plugin_host.h"
#include "akashi/service_registry.h"
#include "core/command_registry.h"
#include "core/event_bus.h"
#include "core/log_service.h"
#include "core/permission_registry.h"
#include "core/text_filter_registry.h"
#include "world/rule_registry.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPluginLoader>


namespace akashi {

PluginManager::PluginManager(ServiceRegistry *f_services, const QString &f_plugin_dir,
                             QObject *parent)
    : QObject(parent),
      m_services(f_services),
      m_plugin_dir(f_plugin_dir)
{}

PluginManager::~PluginManager()
{
    shutdownAll();
}

QString PluginManager::serviceId() const { return QStringLiteral("akashi.plugins"); }
ServiceVersion PluginManager::serviceVersion() const { return {1, 0, 0}; }

bool PluginManager::startPlugins(const QStringList &f_allowlist)
{
    m_allowlist = f_allowlist;
    if (!discover(f_allowlist))
        return false;

    m_load_order = topologicalSort();
    if (m_load_order.isEmpty() && !m_plugins.isEmpty())
        return false;

    for (const QString &l_id : std::as_const(m_load_order)) {
        auto it = m_plugins.find(l_id);
        if (it == m_plugins.end())
            continue;

        PluginEntry &l_entry = it.value();

        if (!validateServices(l_entry)) {
            qWarning() << "Plugin" << l_id << "skipped: required service not available";
            l_entry.info.state = PluginInfo::State::Failed;
            continue;
        }

        l_entry.loader->load();
        QObject *l_obj = l_entry.loader->instance();
        if (!l_obj) {
            qWarning() << "Plugin" << l_id << "failed to load:" << l_entry.loader->errorString();
            l_entry.info.state = PluginInfo::State::Failed;
            continue;
        }

        l_entry.instance = qobject_cast<IPlugin *>(l_obj);
        if (!l_entry.instance) {
            qWarning() << "Plugin" << l_id << "does not implement IPlugin (IID mismatch)";
            l_entry.loader->unload();
            l_entry.info.state = PluginInfo::State::Failed;
            continue;
        }

        if (!l_entry.instance->load(*m_services)) {
            qWarning() << "Plugin" << l_id << "load() returned false";
            l_entry.instance = nullptr;
            l_entry.loader->unload();
            l_entry.info.state = PluginInfo::State::Failed;
            continue;
        }

        l_entry.info.state = PluginInfo::State::Loaded;
        qInfo() << "Plugin loaded:" << l_id << l_entry.info.version.toString();
    }

    for (const QString &l_id : std::as_const(m_load_order)) {
        auto it = m_plugins.find(l_id);
        if (it == m_plugins.end())
            continue;
        PluginEntry &l_entry = it.value();
        if (l_entry.info.state != PluginInfo::State::Loaded)
            continue;

        if (!l_entry.instance->init(*m_services)) {
            qWarning() << "Plugin" << l_id << "init() returned false";
            l_entry.instance->shutdown(*m_services);
            cleanupPlugin(l_id);
            l_entry.instance = nullptr;
            l_entry.loader->unload();
            l_entry.info.state = PluginInfo::State::Failed;
            continue;
        }

        l_entry.info.state = PluginInfo::State::Initialized;
    }

    for (const QString &l_id : std::as_const(m_load_order)) {
        auto it = m_plugins.find(l_id);
        if (it == m_plugins.end())
            continue;
        PluginEntry &l_entry = it.value();
        if (l_entry.info.state != PluginInfo::State::Initialized)
            continue;

        l_entry.instance->started(*m_services);
        l_entry.info.state = PluginInfo::State::Started;
        qInfo() << "Plugin started:" << l_id << l_entry.info.version.toString();
    }

    // The hosts are running now, so they can find their plugin files.
    discoverFromScriptHosts();
    loadDiscoveredScripts();

    return true;
}

// Asks every registered script host to scan the plugin directory and files
// the reported manifests as discovered plugins. Safe to run repeatedly;
// known ids are left alone.
void PluginManager::discoverFromScriptHosts()
{
    const QStringList l_service_ids = m_services->serviceIds();
    for (const QString &l_service_id : l_service_ids) {
        if (!l_service_id.startsWith(QStringLiteral("akashi.script-host."))) {
            continue;
        }
        auto l_host = m_services->resolve<IScriptPluginHost>(l_service_id);
        if (!l_host) {
            continue;
        }

        const QList<PluginInfo> l_manifests = l_host->discoverScriptPlugins(m_plugin_dir);
        for (const PluginInfo &l_manifest : l_manifests) {
            if (m_plugins.contains(l_manifest.id)) {
                continue;
            }
            if (!m_allowlist.isEmpty() && !m_allowlist.contains(l_manifest.id)) {
                qInfo() << "Plugin" << l_manifest.id << "not in allowlist, skipping";
                continue;
            }

            PluginEntry l_entry;
            l_entry.info = l_manifest;
            if (l_entry.info.runtime.isEmpty()) {
                l_entry.info.runtime = l_host->runtime();
            }
            m_plugins.insert(l_manifest.id, l_entry);
            qInfo() << "Discovered script plugin:" << l_manifest.id << l_manifest.version.toString() << "(" + l_entry.info.runtime + ")";
        }
    }
}

// Loads discovered script plugins in dependency order: whoever's
// dependencies are all running loads, until nothing moves anymore.
void PluginManager::loadDiscoveredScripts()
{
    bool l_progress = true;
    while (l_progress) {
        l_progress = false;
        const QStringList l_ids = m_plugins.keys();
        for (const QString &l_id : l_ids) {
            PluginEntry &l_entry = m_plugins[l_id];
            if (!l_entry.isScript() || l_entry.info.state != PluginInfo::State::Discovered) {
                continue;
            }
            bool l_ready = true;
            for (const QString &l_dep : l_entry.info.dependencies) {
                auto dep_it = m_plugins.constFind(l_dep);
                if (dep_it == m_plugins.constEnd() || dep_it.value().info.state != PluginInfo::State::Started) {
                    l_ready = false;
                    break;
                }
            }
            if (!l_ready) {
                continue;
            }
            if (loadPlugin(l_id)) {
                qInfo() << "Script plugin started:" << l_id << l_entry.info.version.toString() << "(" + l_entry.info.runtime + ")";
                l_progress = true;
            }
            else {
                l_entry.info.state = PluginInfo::State::Failed;
            }
        }
    }

    for (auto it = m_plugins.cbegin(); it != m_plugins.cend(); ++it) {
        if (it.value().isScript() && it.value().info.state == PluginInfo::State::Discovered) {
            qWarning() << "Script plugin" << it.key() << "not started: dependencies missing or not running";
        }
    }
}

void PluginManager::shutdownAll()
{
    QList<QString> l_reverse = m_load_order;
    std::reverse(l_reverse.begin(), l_reverse.end());

    for (const QString &l_id : std::as_const(l_reverse)) {
        auto it = m_plugins.find(l_id);
        if (it == m_plugins.end())
            continue;
        PluginEntry &l_entry = it.value();

        if (l_entry.isScript()) {
            if (!l_entry.isActive())
                continue;
            // Registrations go first: the applied-rule sweep needs the
            // plugin's action factories still known, and after the sweep
            // nothing can call into the interpreter the host closes.
            cleanupPlugin(l_id);
            unloadScriptEntry(l_entry);
            l_entry.info.state = PluginInfo::State::Discovered;
            qInfo() << "Script plugin unloaded:" << l_id;
            continue;
        }

        if (!l_entry.instance)
            continue;

        l_entry.instance->shutdown(*m_services);
        cleanupPlugin(l_id);
        l_entry.instance = nullptr;
        l_entry.loader->unload();
        l_entry.info.state = PluginInfo::State::Discovered;
        qInfo() << "Plugin unloaded:" << l_id;
    }

    for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it)
        delete it.value().loader;
    m_plugins.clear();
    m_load_order.clear();
}

bool PluginManager::loadPlugin(const QString &f_id)
{
    auto it = m_plugins.find(f_id);
    if (it == m_plugins.end())
        return false;

    PluginEntry &l_entry = it.value();
    if (l_entry.info.state != PluginInfo::State::Discovered)
        return false;

    // A script plugin's file may have changed since discovery; the host's
    // fresh manifest decides its dependencies and services. The id stays
    // what it was discovered under.
    if (l_entry.isScript()) {
        auto l_host = m_services->resolve<IScriptPluginHost>(QStringLiteral("akashi.script-host.") + l_entry.info.runtime);
        if (!l_host) {
            return false;
        }
        const QList<PluginInfo> l_manifests = l_host->discoverScriptPlugins(m_plugin_dir);
        bool l_found = false;
        for (const PluginInfo &l_manifest : l_manifests) {
            if (l_manifest.id != l_entry.info.id) {
                continue;
            }
            l_entry.info.version = l_manifest.version;
            l_entry.info.entry_path = l_manifest.entry_path;
            l_entry.info.file_path = l_manifest.file_path;
            l_entry.info.dependencies = l_manifest.dependencies;
            l_entry.info.services = l_manifest.services;
            l_found = true;
            break;
        }
        if (!l_found) {
            qWarning() << "Script plugin" << f_id << "is no longer reported by its host";
            return false;
        }
    }

    if (!validateServices(l_entry))
        return false;

    for (const QString &l_dep : l_entry.info.dependencies) {
        auto dep_it = m_plugins.find(l_dep);
        if (dep_it == m_plugins.end() || dep_it.value().info.state != PluginInfo::State::Started)
            return false;
    }

    if (l_entry.isScript()) {
        if (!loadScriptEntry(l_entry))
            return false;
        l_entry.info.state = PluginInfo::State::Started;
        if (!m_load_order.contains(f_id))
            m_load_order.append(f_id);
        qInfo() << "Plugin loaded:" << f_id << l_entry.info.version.toString() << "(" + l_entry.info.runtime + ")";
        return true;
    }

    l_entry.loader->load();
    QObject *l_obj = l_entry.loader->instance();
    if (!l_obj)
        return false;

    l_entry.instance = qobject_cast<IPlugin *>(l_obj);
    if (!l_entry.instance) {
        l_entry.loader->unload();
        return false;
    }

    if (!l_entry.instance->load(*m_services)) {
        l_entry.instance = nullptr;
        l_entry.loader->unload();
        return false;
    }
    l_entry.info.state = PluginInfo::State::Loaded;

    if (!l_entry.instance->init(*m_services)) {
        l_entry.instance->shutdown(*m_services);
        cleanupPlugin(f_id);
        l_entry.instance = nullptr;
        l_entry.loader->unload();
        l_entry.info.state = PluginInfo::State::Discovered;
        return false;
    }
    l_entry.info.state = PluginInfo::State::Initialized;

    l_entry.instance->started(*m_services);
    l_entry.info.state = PluginInfo::State::Started;

    if (!m_load_order.contains(f_id))
        m_load_order.append(f_id);

    // The plugin may be a script host with plugin files of its own; they
    // become visible now and load with /plugin load.
    discoverFromScriptHosts();

    qInfo() << "Plugin loaded:" << f_id << l_entry.info.version.toString();
    return true;
}

bool PluginManager::unloadPlugin(const QString &f_id, bool f_cascade)
{
    auto it = m_plugins.find(f_id);
    if (it == m_plugins.end() || !it.value().isActive())
        return false;

    QStringList l_dependents = dependentsOf(f_id);
    if (!l_dependents.isEmpty()) {
        if (!f_cascade)
            return false;

        for (const QString &l_dep : l_dependents) {
            if (!unloadPlugin(l_dep, true))
                return false;
        }
    }

    PluginEntry &l_entry = it.value();
    if (l_entry.isScript()) {
        // Registrations go first: the applied-rule sweep needs the plugin's
        // action factories still known, and after the sweep nothing can
        // call into the interpreter the host closes.
        cleanupPlugin(f_id);
        unloadScriptEntry(l_entry);
        l_entry.info.state = PluginInfo::State::Discovered;
        m_load_order.removeOne(f_id);
        qInfo() << "Plugin unloaded:" << f_id;
        return true;
    }

    l_entry.instance->shutdown(*m_services);
    cleanupPlugin(f_id);
    l_entry.instance = nullptr;
    l_entry.loader->unload();
    l_entry.info.state = PluginInfo::State::Discovered;
    m_load_order.removeOne(f_id);

    qInfo() << "Plugin unloaded:" << f_id;
    return true;
}

bool PluginManager::reloadPlugin(const QString &f_id)
{
    auto it = m_plugins.find(f_id);
    if (it == m_plugins.end())
        return false;

    if (it.value().isActive()) {
        if (!unloadPlugin(f_id, true))
            return false;
    }

    return loadPlugin(f_id);
}

QList<PluginInfo> PluginManager::plugins() const
{
    QList<PluginInfo> l_result;
    l_result.reserve(m_plugins.size());
    for (auto it = m_plugins.cbegin(); it != m_plugins.cend(); ++it)
        l_result.append(it.value().info);
    return l_result;
}

std::optional<PluginInfo> PluginManager::pluginInfo(const QString &f_id) const
{
    auto it = m_plugins.find(f_id);
    if (it == m_plugins.end())
        return std::nullopt;
    return it.value().info;
}

bool PluginManager::discover(const QStringList &f_allowlist)
{
    QDir l_dir(m_plugin_dir);
    if (!l_dir.exists()) {
        qInfo() << "Plugin directory does not exist:" << m_plugin_dir;
        return true;
    }

    QStringList l_filters;
#ifdef Q_OS_WIN
    l_filters << QStringLiteral("*.dll");
#elif defined(Q_OS_MACOS)
    l_filters << QStringLiteral("*.dylib");
#else
    l_filters << QStringLiteral("*.so");
#endif

    const QStringList l_files = l_dir.entryList(l_filters, QDir::Files);
    for (const QString &l_file : l_files) {
        const QString l_path = l_dir.absoluteFilePath(l_file);
        auto l_loader = new QPluginLoader(l_path, this);

        QJsonObject l_meta = l_loader->metaData();
        if (l_meta.isEmpty()) {
            qWarning() << "No metadata in" << l_file;
            delete l_loader;
            continue;
        }

        QJsonObject l_md = l_meta.value(QStringLiteral("MetaData")).toObject();
        QString l_id = l_md.value(QStringLiteral("id")).toString();
        if (l_id.isEmpty()) {
            qWarning() << "No id in metadata of" << l_file;
            delete l_loader;
            continue;
        }

        if (!f_allowlist.isEmpty() && !f_allowlist.contains(l_id)) {
            qInfo() << "Plugin" << l_id << "not in allowlist, skipping";
            delete l_loader;
            continue;
        }

        if (m_plugins.contains(l_id)) {
            qWarning() << "Duplicate plugin id" << l_id << "in" << l_file;
            delete l_loader;
            continue;
        }

        PluginInfo l_info;
        l_info.id = l_id;
        l_info.file_path = l_path;

        QJsonObject l_ver_obj = l_md.value(QStringLiteral("version")).toObject();
        if (!l_ver_obj.isEmpty()) {
            l_info.version = {l_ver_obj.value(QStringLiteral("major")).toInt(1),
                              l_ver_obj.value(QStringLiteral("minor")).toInt(0),
                              l_ver_obj.value(QStringLiteral("patch")).toInt(0)};
        }
        else {
            QString l_ver_str = l_md.value(QStringLiteral("version")).toString();
            if (!l_ver_str.isEmpty()) {
                QStringList l_parts = l_ver_str.split(QLatin1Char('.'));
                l_info.version = {l_parts.value(0).toInt(),
                                  l_parts.value(1).toInt(),
                                  l_parts.value(2).toInt()};
            }
        }

        const QJsonArray l_deps = l_md.value(QStringLiteral("dependencies")).toArray();
        for (const auto &l_val : l_deps)
            l_info.dependencies.append(l_val.toString());

        const QJsonArray l_svcs = l_md.value(QStringLiteral("services")).toArray();
        for (const auto &l_val : l_svcs)
            l_info.services.append(l_val.toString());

        PluginEntry l_entry;
        l_entry.info = l_info;
        l_entry.loader = l_loader;

        m_plugins.insert(l_id, l_entry);
        qInfo() << "Discovered plugin:" << l_id << l_info.version.toString();
    }

    return true;
}

std::optional<PluginInfo> PluginManager::parseScriptHeader(const QString &f_file_path)
{
    QFile l_file(f_file_path);
    if (!l_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return std::nullopt;
    }
    // The declaration header must sit near the top of the file.
    const QString l_head = QString::fromUtf8(l_file.read(4096));
    const int l_marker = l_head.indexOf(QStringLiteral("akashi-plugin"));
    if (l_marker < 0) {
        // Not a plugin, just a script somebody parked here.
        return std::nullopt;
    }

    // The JSON object after the marker, with matched braces so nested
    // objects survive.
    const int l_open = l_head.indexOf(QLatin1Char('{'), l_marker);
    int l_close = -1;
    int l_depth = 0;
    for (int i = l_open; l_open >= 0 && i < l_head.size(); i++) {
        if (l_head[i] == QLatin1Char('{')) {
            l_depth++;
        }
        else if (l_head[i] == QLatin1Char('}')) {
            if (--l_depth == 0) {
                l_close = i;
                break;
            }
        }
    }
    if (l_close < 0) {
        qWarning() << "No declaration object after the akashi-plugin marker in" << f_file_path;
        return std::nullopt;
    }

    QJsonParseError l_error;
    const QJsonDocument l_doc = QJsonDocument::fromJson(l_head.mid(l_open, l_close - l_open + 1).toUtf8(), &l_error);
    if (l_error.error != QJsonParseError::NoError || !l_doc.isObject()) {
        qWarning() << "Unreadable declaration header in" << f_file_path << ":" << l_error.errorString();
        return std::nullopt;
    }
    const QJsonObject l_md = l_doc.object();

    const QFileInfo l_file_info(f_file_path);
    QString l_runtime = l_md.value(QStringLiteral("runtime")).toString();
    if (l_runtime.isEmpty()) {
        const QString l_suffix = l_file_info.suffix().toLower();
        l_runtime = l_suffix == QStringLiteral("py") ? QStringLiteral("python")
                                                     : l_suffix == QStringLiteral("lua") ? QStringLiteral("lua")
                                                                                         : QString();
    }
    if (l_runtime.isEmpty()) {
        qWarning() << "No runtime for script plugin" << f_file_path;
        return std::nullopt;
    }

    // Everything but the marker itself has a sensible default, so the
    // smallest plugin is one script file with a one-line header.
    PluginInfo l_info;
    l_info.id = l_md.value(QStringLiteral("id")).toString();
    if (l_info.id.isEmpty()) {
        l_info.id = l_file_info.completeBaseName();
    }
    l_info.file_path = f_file_path;
    l_info.entry_path = f_file_path;
    l_info.runtime = l_runtime;
    l_info.version = {1, 0, 0};
    const QString l_ver_str = l_md.value(QStringLiteral("version")).toString();
    if (!l_ver_str.isEmpty()) {
        const QStringList l_parts = l_ver_str.split(QLatin1Char('.'));
        l_info.version = {l_parts.value(0).toInt(), l_parts.value(1).toInt(), l_parts.value(2).toInt()};
    }

    const QJsonArray l_deps = l_md.value(QStringLiteral("dependencies")).toArray();
    for (const auto &l_val : l_deps) {
        l_info.dependencies.append(l_val.toString());
    }
    // The matching host is always a dependency; authors never write it out.
    const QString l_host_id = QStringLiteral("akashi.") + l_runtime + QStringLiteral("-host");
    if (!l_info.dependencies.contains(l_host_id)) {
        l_info.dependencies.prepend(l_host_id);
    }

    const QJsonArray l_svcs = l_md.value(QStringLiteral("services")).toArray();
    for (const auto &l_val : l_svcs) {
        l_info.services.append(l_val.toString());
    }

    return l_info;
}

bool PluginManager::loadScriptEntry(PluginEntry &f_entry)
{
    auto l_host = m_services->resolve<IScriptPluginHost>(QStringLiteral("akashi.script-host.") + f_entry.info.runtime);
    if (!l_host) {
        qWarning() << "Script plugin" << f_entry.info.id << "has no running host for runtime" << f_entry.info.runtime;
        return false;
    }
    return l_host->loadScriptPlugin(f_entry.info.id, f_entry.info.entry_path);
}

void PluginManager::unloadScriptEntry(PluginEntry &f_entry)
{
    auto l_host = m_services->resolve<IScriptPluginHost>(QStringLiteral("akashi.script-host.") + f_entry.info.runtime);
    if (l_host) {
        l_host->unloadScriptPlugin(f_entry.info.id);
    }
}

QStringList PluginManager::topologicalSort() const
{
    QHash<QString, int> l_in_degree;
    QHash<QString, QStringList> l_adjacency;

    for (auto it = m_plugins.cbegin(); it != m_plugins.cend(); ++it) {
        const QString &l_id = it.key();
        l_in_degree[l_id] = 0;
        l_adjacency[l_id] = {};
    }

    for (auto it = m_plugins.cbegin(); it != m_plugins.cend(); ++it) {
        const QString &l_id = it.key();
        for (const QString &l_dep : it.value().info.dependencies) {
            if (!m_plugins.contains(l_dep)) {
                qWarning() << "Plugin" << l_id << "depends on missing plugin" << l_dep << "- skipping";
                return {};
            }
            l_adjacency[l_dep].append(l_id);
            l_in_degree[l_id]++;
        }
    }

    QStringList l_queue;
    for (auto it = l_in_degree.cbegin(); it != l_in_degree.cend(); ++it) {
        if (it.value() == 0)
            l_queue.append(it.key());
    }
    l_queue.sort();

    QStringList l_result;
    while (!l_queue.isEmpty()) {
        QString l_current = l_queue.takeFirst();
        l_result.append(l_current);

        QStringList l_next_candidates;
        for (const QString &l_neighbor : std::as_const(l_adjacency[l_current])) {
            l_in_degree[l_neighbor]--;
            if (l_in_degree[l_neighbor] == 0)
                l_next_candidates.append(l_neighbor);
        }
        l_next_candidates.sort();
        l_queue.append(l_next_candidates);
    }

    if (l_result.size() != m_plugins.size()) {
        qCritical() << "Circular dependency detected among plugins";
        return {};
    }

    return l_result;
}

bool PluginManager::validateServices(const PluginEntry &f_entry) const
{
    for (const QString &l_svc : f_entry.info.services) {
        if (!m_services->isAvailable(l_svc)) {
            qWarning() << "Plugin" << f_entry.info.id << "requires service" << l_svc << "which is not available";
            return false;
        }
    }
    return true;
}

void PluginManager::cleanupPlugin(const QString &f_id)
{
    // Listeners sweep the plugin's applied rules while the registry still
    // knows which actions the plugin owned.
    Q_EMIT pluginAboutToUnload(f_id);

    m_services->unregisterServicesOwnedBy(f_id);

    auto l_commands = m_services->resolve<CommandRegistry>(QStringLiteral("akashi.commands"));
    if (l_commands) l_commands->unregisterAll(f_id);

    auto l_events = m_services->resolve<EventBus>(QStringLiteral("akashi.events"));
    if (l_events) l_events->unsubscribeAll(f_id);

    auto l_filters = m_services->resolve<TextFilterRegistry>(QStringLiteral("akashi.textfilters"));
    if (l_filters) l_filters->unregisterAll(f_id);

    auto l_permissions = m_services->resolve<PermissionRegistry>(QStringLiteral("akashi.permissions"));
    if (l_permissions) {
        l_permissions->unregisterAllPermissions(f_id);
        l_permissions->unregisterAllResolvers(f_id);
    }

    auto l_log = m_services->resolve<LogService>(QStringLiteral("akashi.log"));
    if (l_log) l_log->unregisterAll(f_id);

    auto l_rules = m_services->resolve<RuleRegistry>(QStringLiteral("akashi.rules"));
    if (l_rules) l_rules->unregisterActions(f_id);
}

QStringList PluginManager::dependentsOf(const QString &f_id) const
{
    QStringList l_result;
    for (auto it = m_plugins.cbegin(); it != m_plugins.cend(); ++it) {
        if (it.value().isActive() && it.value().info.dependencies.contains(f_id))
            l_result.append(it.key());
    }
    l_result.sort();
    return l_result;
}

} // namespace akashi
