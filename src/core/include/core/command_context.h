#pragma once

#include "akashi_core_export.h"

#include <QHostAddress>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace akashi {
class ClientSession;
class PlayerState;
}
class ServerContext;

namespace akashi {
class Area;
class ServiceRegistry;
}

namespace akashi {

class AKASHI_CORE_EXPORT TargetPlayer
{
  public:
    explicit TargetPlayer(akashi::ClientSession *f_client);

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

    // The characters this person plays (core/player_state.h), oldest first.
    // A session has exactly one today; a richer protocol may
    // voice several. There is deliberately no active-character accessor:
    // plugins iterate the list or pick a PlayerState by its id. The states
    // live and die with the session: read them right away, never store them.
    QList<PlayerState *> players() const;

  private:
    akashi::ClientSession *m_client;
};

class AKASHI_CORE_EXPORT CommandContext
{
  public:
    CommandContext(akashi::ClientSession *f_invoker, ServerContext *f_server, QStringList f_arguments);

    int clientId() const;
    QString name() const;
    QString character() const;
    QString characterName() const;
    int areaId() const;
    QString areaName() const;
    // The area the invoker stands in.
    Area *area() const;
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
    ServerContext *server() const;

  private:
    akashi::ClientSession *m_invoker;
    ServerContext *m_server;
    QStringList m_arguments;
};

} // namespace akashi
