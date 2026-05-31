#include "core/plugin_manager.h"

#include "akashi/plugin.h"
#include "akashi/service_registry.h"
#include "core/command_registry.h"
#include "core/event_bus.h"
#include "core/log_service.h"
#include "core/permission_registry.h"
#include "core/text_filter_registry.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QJsonArray>
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
        qInfo() << "Plugin loaded:" << l_id;
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
        qInfo() << "Plugin started:" << l_id;
    }

    return true;
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

    if (!validateServices(l_entry))
        return false;

    for (const QString &l_dep : l_entry.info.dependencies) {
        auto dep_it = m_plugins.find(l_dep);
        if (dep_it == m_plugins.end() || dep_it.value().info.state != PluginInfo::State::Started)
            return false;
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

    return true;
}

bool PluginManager::unloadPlugin(const QString &f_id, bool f_cascade)
{
    auto it = m_plugins.find(f_id);
    if (it == m_plugins.end() || !it.value().instance)
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
    l_entry.instance->shutdown(*m_services);
    cleanupPlugin(f_id);
    l_entry.instance = nullptr;
    l_entry.loader->unload();
    l_entry.info.state = PluginInfo::State::Discovered;
    m_load_order.removeOne(f_id);

    return true;
}

bool PluginManager::reloadPlugin(const QString &f_id)
{
    auto it = m_plugins.find(f_id);
    if (it == m_plugins.end())
        return false;

    if (it.value().instance) {
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
    m_services->unregisterServicesOwnedBy(f_id);

    auto l_commands = m_services->resolve<CommandRegistry>(QStringLiteral("akashi.commands"));
    if (l_commands) l_commands->unregisterAll(f_id);

    auto l_events = m_services->resolve<EventBus>(QStringLiteral("akashi.events"));
    if (l_events) l_events->unsubscribeAll(f_id);

    auto l_filters = m_services->resolve<TextFilterRegistry>(QStringLiteral("akashi.filters"));
    if (l_filters) l_filters->unregisterAll(f_id);

    auto l_permissions = m_services->resolve<PermissionRegistry>(QStringLiteral("akashi.permissions"));
    if (l_permissions) {
        l_permissions->unregisterAllPermissions(f_id);
        l_permissions->unregisterAllResolvers(f_id);
    }

    auto l_log = m_services->resolve<LogService>(QStringLiteral("akashi.log"));
    if (l_log) l_log->unregisterAll(f_id);
}

QStringList PluginManager::dependentsOf(const QString &f_id) const
{
    QStringList l_result;
    for (auto it = m_plugins.cbegin(); it != m_plugins.cend(); ++it) {
        if (it.value().instance && it.value().info.dependencies.contains(f_id))
            l_result.append(it.key());
    }
    l_result.sort();
    return l_result;
}

} // namespace akashi
