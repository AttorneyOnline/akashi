#ifndef CORE_COMMAND_CONTEXT_H
#define CORE_COMMAND_CONTEXT_H

#include "akashi_core_export.h"

#include <QHostAddress>
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

    void sendPacket(const QString &f_header, const QStringList &f_fields);
    void closeSocket();
    QHostAddress remoteIp() const;
    QString hwid() const;
    QString characterName() const;
    bool isPmMuted() const;
    void setTestimonySaving(bool f_state);

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
    QString characterName() const;
    int areaId() const;
    QString areaName() const;
    QString ipid() const;
    QString hwid() const;
    bool isAuthenticated() const;
    bool canPerform(const QString &f_permission) const;

    QString aclRoleId() const;
    void setAclRoleId(const QString &f_role_id);
    QString moderatorName() const;
    void setModeratorName(const QString &f_name);
    void setAuthenticated(bool f_state);
    void setInLoginPrompt(bool f_state);

    int argc() const;
    const QStringList &arguments() const;
    QString argument(int f_index) const;
    std::optional<int> argumentAsInt(int f_index) const;

    void reply(const QString &f_message);
    void replyToArea(const QString &f_message);
    void replyToServer(const QString &f_message);
    void sendPacket(const QString &f_header, const QStringList &f_fields);

    std::optional<TargetPlayer> resolveTarget(int f_argument_index = 0);

    static int genRand(int f_min, int f_max);

    akashi::ServiceRegistry *services() const;
    Server *server() const;

  private:
    AOClient *m_invoker;
    Server *m_server;
    QStringList m_arguments;
};

} // namespace akashi

#endif // CORE_COMMAND_CONTEXT_H
