#ifndef CORE_COMMAND_CONTEXT_H
#define CORE_COMMAND_CONTEXT_H

#include "akashi_core_export.h"

#include <QString>
#include <QStringList>

#include <optional>

class AOClient;
class Server;

namespace akashi {
class ServiceRegistry;
}

namespace akashi {

class AKASHI_CORE_EXPORT TargetPlayer
{
  public:
    explicit TargetPlayer(AOClient *f_client);

    int clientId() const;
    QString name() const;
    QString character() const;
    int areaId() const;
    QString ipid() const;
    bool isAuthenticated() const;

    void reply(const QString &f_message);

    bool hasSanction(const QString &f_sanction_id) const;
    void setSanction(const QString &f_sanction_id, bool f_active);

    void changeArea(int f_area_id);
    void forceCharacterSelect();

  private:
    AOClient *m_client;
};

class AKASHI_CORE_EXPORT CommandContext
{
  public:
    CommandContext(AOClient *f_invoker, Server *f_server, QStringList f_arguments);

    int clientId() const;
    QString name() const;
    QString character() const;
    int areaId() const;
    QString areaName() const;
    QString ipid() const;
    QString hwid() const;
    bool isAuthenticated() const;

    int argc() const;
    const QStringList &arguments() const;
    QString argument(int f_index) const;
    std::optional<int> argumentAsInt(int f_index) const;

    void reply(const QString &f_message);
    void replyToArea(const QString &f_message);
    void replyToServer(const QString &f_message);

    std::optional<TargetPlayer> resolveTarget(int f_argument_index = 0);

    akashi::ServiceRegistry *services() const;
    Server *server() const;

  private:
    AOClient *m_invoker;
    Server *m_server;
    QStringList m_arguments;
};

} // namespace akashi

#endif // CORE_COMMAND_CONTEXT_H
