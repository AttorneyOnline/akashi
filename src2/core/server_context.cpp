#include "core/server_context.h"

#include "config_manager.h"
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
    if (!ConfigManager::verifyServerConfig()) {
        qCritical() << "config.ini is invalid!";
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
