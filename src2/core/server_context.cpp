#include "core/server_context.h"

#include "akashi/config_store.h"
#include "config_manager.h"
#include "core/server_config_entries.h"
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
    const bool l_declared = m_config_store->declare("config", serverConfigEntries()) &&
                            m_config_store->declare("discord", discordConfigEntries());
    ConfigManager::setStore(m_config_store);
    if (!l_declared || !ConfigManager::verifyServerConfig()) {
        qCritical() << "The server configuration is invalid!";
        return ExitCode::InvalidConfig;
    }

    m_server = new Server(ConfigManager::serverPort(), this);
    setStage(Stage::ServicesConstructed);

    // Server::start() still loads content and opens the port in one go.
    // The stages in between become real once construction moves here.
    ExitCode code = m_server->start();
    if (code != ExitCode::Ok) {
        return code;
    }
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
    emit stageChanged(m_stage);
}
