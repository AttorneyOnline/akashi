#include "pluginmanager.h"
#include "akashiplugin.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QPluginLoader>

PluginManager::PluginManager(ServiceRegistry *registry, QObject *parent) : QObject(parent), m_registry(registry)
{
    if (!m_registry) {
        qWarning() << "PluginManager created with null ServiceRegistry!";
    }
    qDebug() << "PluginManager initialized";
}

PluginManager::~PluginManager()
{
    qDebug() << "Shutting down PluginManager";
    unloadAllPlugins();
}

bool PluginManager::loadPlugin(const QString &path)
{
    QFileInfo fileInfo(path);
    if (!fileInfo.exists()) {
        qWarning() << "Plugin file does not exist:" << path;
        return false;
    }

    QPluginLoader *loader = new QPluginLoader(fileInfo.absoluteFilePath(), this);

    if (!loader->load()) {
        qWarning() << "Failed to load plugin:" << path << "-" << loader->errorString();
        loader->deleteLater();
        return false;
    }

    AkashiPlugin *interface = qobject_cast<AkashiPlugin *>(loader->instance());
    if (!interface) {
        qWarning() << "Plugin does not implement AkashiPlugin interface:" << path;
        loader->unload();
        loader->deleteLater();
        return false;
    }

    const QString pluginName = interface->name();

    if (m_plugins.contains(pluginName)) {
        qWarning() << "Plugin already loaded:" << pluginName;
        loader->unload();
        loader->deleteLater();
        return false;
    }

    if (!interface->initialize(m_registry)) {
        qWarning() << "Plugin initialization failed:" << pluginName;
        loader->unload();
        loader->deleteLater();
        return false;
    }

    m_plugins.insert(pluginName, loader);

    qInfo() << "Plugin loaded:" << pluginName;
    emit pluginLoaded(pluginName);

    return true;
}

bool PluginManager::unloadPlugin(const QString &name)
{
    QPluginLoader *loader = m_plugins.value(name, nullptr);
    if (!loader) {
        qWarning() << "Plugin not found:" << name;
        return false;
    }

    AkashiPlugin *interface = qobject_cast<AkashiPlugin *>(loader->instance());
    if (interface) {
        interface->shutdown();
    }

    loader->unload();
    loader->deleteLater();
    m_plugins.remove(name);

    qInfo() << "Plugin unloaded:" << name;
    emit pluginUnloaded(name);

    return true;
}

void PluginManager::unloadAllPlugins()
{
    qDebug() << "Unloading all plugins...";

    const QStringList pluginNames = m_plugins.keys();
    for (const QString &name : pluginNames) {
        unloadPlugin(name);
    }
}

bool PluginManager::loadPluginsFromDirectory(const QString &directory)
{
    QDir pluginDir(directory);

    if (!pluginDir.exists()) {
        qWarning() << "Plugin directory does not exist:" << directory;
        return false;
    }

    qDebug() << "Loading plugins from:" << pluginDir.absolutePath();

    QStringList nameFilters;
#ifdef Q_OS_WIN
    nameFilters << "*.dll";
#elif defined(Q_OS_MACOS)
    nameFilters << "*.dylib" << "*.so";
#else
    nameFilters << "*.so";
#endif

    pluginDir.setNameFilters(nameFilters);
    const QStringList pluginFiles = pluginDir.entryList(
        QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks);

    if (pluginFiles.isEmpty()) {
        qWarning() << "No plugin files found in" << directory;
        return false;
    }

    int successCount = 0;
    for (const QString &fileName : pluginFiles) {
        if (loadPlugin(pluginDir.absoluteFilePath(fileName))) {
            ++successCount;
        }
    }

    qInfo() << "Loaded" << successCount << "of" << pluginFiles.size() << "plugins";
    return successCount > 0;
}

bool PluginManager::isPluginLoaded(const QString &name) const
{
    return m_plugins.contains(name);
}

QStringList PluginManager::getLoadedPlugins() const
{
    return m_plugins.keys();
}
