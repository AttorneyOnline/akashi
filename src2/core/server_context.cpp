#include "core/server_context.h"

#include "akashi/config_store.h"
#include "akashi/database_service.h"
#include "akashi/network_service.h"
#include "akashi/service_registry.h"
#include "config_manager.h"
#include "proto/chat.h"
#include "proto/handshake.h"
#include "proto/ic.h"
#include "proto/packet_service.h"
#include "server.h"

#include <QDebug>

ServerContext::ServerContext(QObject *parent) :
    QObject(parent)
{}

ServerContext::~ServerContext()
{
    shutdown();
}

ExitCode ServerContext::start()
{
    setStage(Stage::Configuring);
    m_config_store = new akashi::ConfigStore(akashi::ConfigStore::resolveRootPath(), this);
    if (!ConfigManager::setStore(m_config_store) || !ConfigManager::verifyServerConfig()) {
        qCritical() << "The server configuration is invalid!";
        return ExitCode::InvalidConfig;
    }

    m_database_service = new akashi::DatabaseService(QStringLiteral("data"), this);
    if (!m_database_service->open(ConfigManager::path("akashi.db"))) {
        return ExitCode::DatabaseError;
    }

    // Shared services the server and plugins can consume by id.
    m_services = new akashi::ServiceRegistry(this);
    m_services->registerService(std::make_shared<akashi::NetworkService>());

    // The packet pipeline, preloaded with the packet families that moved over.
    auto l_packets = std::make_shared<akashi::PacketService>();
    akashi::registerHandshakePackets(l_packets->handlers(), l_packets->codecs());
    akashi::registerChatPackets(l_packets->handlers(), l_packets->codecs());
    akashi::registerIcPackets(l_packets->handlers(), l_packets->codecs());
    m_services->registerService(l_packets);

    m_server = new Server(ConfigManager::serverPort(), m_database_service, m_services, this);
    setStage(Stage::ServicesConstructed);

    // Server::start() still loads content and opens the port in one go.
    // The stages in between become real once construction moves here.
    ExitCode code = m_server->start();
    if (code != ExitCode::Ok) {
        return code;
    }

    // Nightly database housekeeping, held back while too many players are on.
    std::function<bool()> l_busy_check;
    const int l_max_players = ConfigManager::maintenanceMaxPlayers();
    if (l_max_players >= 0) {
        Server *l_server = m_server;
        l_busy_check = [l_server, l_max_players] { return l_server->playerCount() > l_max_players; };
    }
    m_database_service->scheduleMaintenance(ConfigManager::maintenanceTime(), ConfigManager::maintenanceVacuum(), l_busy_check);
    setStage(Stage::ContentLoaded);
    setStage(Stage::PluginsLoaded);
    setStage(Stage::Listening);
    setStage(Stage::Running);
    return ExitCode::Ok;
}

void ServerContext::shutdown()
{
    if (!m_server) {
        return;
    }
    setStage(Stage::Draining);
    delete m_server;
    m_server = nullptr;
    setStage(Stage::Stopped);
}

Server *ServerContext::server() const
{
    return m_server;
}

akashi::ConfigStore *ServerContext::configStore() const
{
    return m_config_store;
}

akashi::ServiceRegistry *ServerContext::services() const
{
    return m_services;
}

ServerContext::Stage ServerContext::stage() const
{
    return m_stage;
}

void ServerContext::setStage(Stage f_stage)
{
    if (m_stage == f_stage) {
        return;
    }
    m_stage = f_stage;
    Q_EMIT stageChanged(m_stage);
}
