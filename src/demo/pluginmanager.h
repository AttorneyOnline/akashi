#pragma once

#include <QMap>
#include <QObject>

#include <service.h>

class QPluginLoader;
class FileSystemSupervisor;

class PluginManager : public Service
{
    Q_OBJECT
    Q_CLASSINFO("Author", "Salanto");
    Q_CLASSINFO("Version", "1.0.0");
    Q_CLASSINFO("Identifier", "akashi.pluginmanager")

  public:
    explicit PluginManager(ServiceRegistry *f_registry, QObject *f_parent = nullptr);

    void loadAllPlugins();
    QStringList loadedPlugins() const;

  private:
    bool loadPlugin(const QString &f_pluginPath);
    void unloadPlugin(const QString &f_identifier);

    ServiceRegistry *m_registry;
    FileSystemSupervisor *fs;
    QMap<QString, QPluginLoader *> m_loaders;
};
