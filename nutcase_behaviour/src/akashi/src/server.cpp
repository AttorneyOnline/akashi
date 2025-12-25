#include "server.h"
#include "discordhook.h"
#include "packetregistry.h"
#include "pluginmanager.h"
#include "serviceregistry.h"

#include "packet_ms.h"

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
    m_packet_registry = new PacketRegistry(m_registry, this);
    m_packet_registry->registerPacket("AO2", "2.11.0", "MS", [](const PacketData &data) -> Packet * {
        return new ServerPacket::MS_V26(data);
    });
    m_plugins = new PluginManager(m_registry, this);

    m_plugins->loadPluginsFromDirectory();
}
