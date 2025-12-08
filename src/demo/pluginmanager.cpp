#include "pluginmanager.h"
#include "filesystemsupervisor.h"
#include "iplugin.h"
#include "serviceregistry.h"

#include <QFileInfo>
#include <QPluginLoader>

PluginManager::PluginManager(ServiceRegistry *f_registry, QObject *parent) : m_registry{f_registry},
                                                                             Service{parent}
{
    if (m_registry->hasService("akashi.filesystemsupervisor")) {
        fs = m_registry->getService<FileSystemSupervisor>("akashi.filesystemsupervisor");
    }
    m_registry->registerService(this);
    loadAllPlugins();
}

void PluginManager::loadAllPlugins()
{
    qDebug() << "Loading all available plugins.";
    const QStringList plugin_paths = fs->directoryContents("plugins", FileSystemSupervisor::SERVER);
    for (const QString &plugin_path : plugin_paths) {
        if (!loadPlugin(plugin_path)) {
            qDebug() << "Failed to load plugin at" << plugin_path;
            continue;
        }
        qDebug() << "Loaded plugin at" << plugin_path;
    }
}

bool PluginManager::loadPlugin(const QString &f_pluginPath)
{
    QPluginLoader *loader = new QPluginLoader(f_pluginPath, this);
    QObject *instance = loader->instance();
    IPlugin *plugin = dynamic_cast<IPlugin *>(instance);
    return plugin->initialize(m_registry);
}

void PluginManager::unloadPlugin(const QString &f_identifier)
{
    QPluginLoader *l_loader = m_loaders.value(f_identifier);
    IPlugin *plugin = dynamic_cast<IPlugin *>(l_loader->instance());
    plugin->shutdown();
    l_loader->deleteLater();
}

QStringList PluginManager::loadedPlugins() const
{
    return m_loaders.keys();
}
