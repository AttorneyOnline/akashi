#include "core/server_context.h"

#include "akashi/config_store.h"
#include "akashi/database_service.h"
#include "akashi/network_service.h"
#include "akashi/service_registry.h"
#include "core/config_loading.h"
#include "core/server_settings.h"
#include "proto/area_music.h"
#include "proto/chat.h"
#include "proto/handshake.h"
#include "proto/ic.h"
#include "proto/moderation.h"
#include "proto/packet_service.h"
#include "server.h"

#include <QDebug>
#include <QFileInfo>

ServerContext::ServerContext(QObject *parent) :
    QObject(parent)
{}

ServerContext::~ServerContext()
{
    shutdown();
}

static bool fileExists(const QFileInfo &f) { return f.exists() && f.isFile(); }
static bool dirExists(const QFileInfo &f) { return f.exists() && f.isDir(); }

static bool verifyServerConfig(akashi::ConfigStore *f_store, ServerSettings *f_settings)
{
    auto path = [&](const QString &f) { return f_store->filePath(f); };

    QStringList l_dirs{path(""), path("text/")};
    for (const QString &d : l_dirs) {
        if (!dirExists(QFileInfo(d))) {
            qCritical() << d + " does not exist!";
            return false;
        }
    }

    // Trigger ini-to-json migration before checking file existence.
    QSettings *l_areas = f_store->settings("areas");

    QStringList l_files{path("config.json"), path("areas.json"), path("backgrounds.txt"),
        path("characters.txt"), path("music.json"), path("discord.json"),
        path("text/8ball.txt"), path("text/gimp.txt"), path("text/praise.txt"),
        path("text/reprimands.txt"), path("text/cdns.txt"), path("ipbans.json")};
    for (const QString &f : l_files) {
        if (!fileExists(QFileInfo(f))) {
            qCritical() << f + " does not exist!";
            return false;
        }
    }

    if (l_areas->childGroups().length() < 1) {
        qCritical() << "areas.json is invalid!";
        return false;
    }

    const int l_soft = f_settings->packet_rate_limit_soft();
    const int l_hard = f_settings->packet_rate_limit_hard();
    if (l_soft > 0 && l_hard <= l_soft) {
        qCritical("packet_rate_limit_hard must be greater than packet_rate_limit_soft!");
        return false;
    }
    if (l_soft <= 0)
        qWarning("packet_rate_limit_soft is 0 or less, warning threshold is disabled!");
    if (l_hard <= 0)
        qWarning("packet_rate_limit_hard is 0 or less, rate limiting is disabled!");

    return true;
}

static int resolveServerPort(akashi::ConfigStore *f_store, ServerSettings *f_settings)
{
    if (f_store->settings("config")->contains("Options/webao_port")) {
        qWarning("webao_port is deprecated, use port instead");
        return f_settings->webao_port();
    }
    return f_settings->port();
}

ExitCode ServerContext::start()
{
    setStage(Stage::Configuring);
    m_config_store = new akashi::ConfigStore(akashi::ConfigStore::resolveRootPath(), this);

    auto l_server_settings = new ServerSettings(m_config_store);
    auto l_discord_settings = new DiscordSettings(m_config_store);
    const bool l_valid = l_server_settings->declare() && l_discord_settings->declare();

    m_config_store->settings("acl_roles");

    if (!l_valid || !verifyServerConfig(m_config_store, l_server_settings)) {
        qCritical() << "The server configuration is invalid!";
        delete l_server_settings;
        delete l_discord_settings;
        return ExitCode::InvalidConfig;
    }

    m_database_service = new akashi::DatabaseService(QStringLiteral("data"), this);
    if (!m_database_service->open(m_config_store->filePath("akashi.db"))) {
        delete l_server_settings;
        delete l_discord_settings;
        return ExitCode::DatabaseError;
    }

    m_services = new akashi::ServiceRegistry(this);
    m_services->registerService(std::make_shared<akashi::NetworkService>());

    auto l_packets = std::make_shared<akashi::PacketService>();
    akashi::registerHandshakePackets(l_packets->handlers(), l_packets->codecs());
    akashi::registerChatPackets(l_packets->handlers(), l_packets->codecs());
    akashi::registerIcPackets(l_packets->handlers(), l_packets->codecs());
    akashi::registerAreaMusicPackets(l_packets->handlers(), l_packets->codecs());
    akashi::registerModerationPackets(l_packets->handlers(), l_packets->codecs());
    m_services->registerService(l_packets);

    const int l_port = resolveServerPort(m_config_store, l_server_settings);
    // Server takes ownership of the settings objects.
    delete l_server_settings;
    delete l_discord_settings;
    m_server = new Server(l_port, m_config_store, m_database_service, m_services, this);
    setStage(Stage::ServicesConstructed);

    ExitCode code = m_server->start();
    if (code != ExitCode::Ok) {
        return code;
    }

    std::function<bool()> l_busy_check;
    const int l_max_players = m_server->serverSettings()->maintenance_max_players();
    if (l_max_players >= 0) {
        Server *l_server = m_server;
        l_busy_check = [l_server, l_max_players] { return l_server->playerCount() > l_max_players; };
    }
    m_database_service->scheduleMaintenance(
        m_server->serverSettings()->maintenance_time(),
        m_server->serverSettings()->maintenance_vacuum(),
        l_busy_check);
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
