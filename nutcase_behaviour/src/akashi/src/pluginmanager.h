#pragma once

#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>

class ServiceRegistry;
class QPluginLoader;

class PluginManager : public QObject
{
    Q_OBJECT

  public:
    explicit PluginManager(ServiceRegistry *registry, QObject *parent = nullptr);
    ~PluginManager() override;

    bool loadPlugin(const QString &path);
    bool unloadPlugin(const QString &name);
    void unloadAllPlugins();

    bool loadPluginsFromDirectory(const QString &directory = "./plugins");
    bool isPluginLoaded(const QString &name) const;
    QStringList getLoadedPlugins() const;

  signals:
    void pluginLoaded(const QString &name);
    void pluginUnloaded(const QString &name);

  private:
    ServiceRegistry *m_registry;
    QMap<QString, QPluginLoader *> m_plugins;
};
