#include "server.h"
#include "discordhook.h"
#include "pluginmanager.h"
#include "serviceregistry.h"

Server::Server(QObject *parent)
    : QObject{parent}
{
    m_registry = new ServiceRegistry(this);

    initializeServices();
}

void Server::initializeServices()
{
    QNetworkAccessManager *l_network = new QNetworkAccessManager(this);
    m_network_service = new ServiceWrapper<QNetworkAccessManager>("Salanto",
                                                                  "1.0.0",
                                                                  "qt.network.manager",
                                                                  l_network,
                                                                  m_registry,
                                                                  this);

    m_discord_hook = new DiscordHook(m_registry, this);
    m_plugins = new PluginManager(m_registry, this);

    m_plugins->loadPluginsFromDirectory();
}
