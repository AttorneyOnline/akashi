#ifndef SERVER_CONTEXT_H
#define SERVER_CONTEXT_H

#include "akashi_core_export.h"
#include "core/exit_code.h"

#include <QObject>

class Server;

namespace akashi {
class ConfigStore;
}

// Owns the server and moves it through its lifecycle stages.
class AKASHI_CORE_EXPORT ServerContext : public QObject
{
    Q_OBJECT

  public:
    enum class Stage
    {
        Configuring,
        ServicesConstructed,
        ContentLoaded,
        PluginsLoaded,
        Listening,
        Running,
        Draining,
        Stopped,
    };
    Q_ENUM(Stage)

    explicit ServerContext(QObject *parent = nullptr);
    ~ServerContext();

    // Builds and starts the server. Returns the exit code describing why startup failed.
    ExitCode start();

    // Stops the server. Safe to call more than once.
    void shutdown();

    Server *server() const;
    akashi::ConfigStore *configStore() const;
    Stage stage() const;

  signals:
    void stageChanged(ServerContext::Stage f_stage);

  private:
    void setStage(Stage f_stage);

    Server *m_server = nullptr;
    akashi::ConfigStore *m_config_store = nullptr;
    Stage m_stage = Stage::Configuring;
};

#endif // SERVER_CONTEXT_H
