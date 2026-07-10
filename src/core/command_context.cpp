#include "core/command_context.h"

#include "core/client_session.h"
#include "core/server_context.h"

#include <QRandomGenerator>

namespace akashi {

// -- TargetPlayer --

TargetPlayer::TargetPlayer(akashi::ClientSession *f_client) :
    m_client(f_client)
{}

int TargetPlayer::clientId() const { return m_client->clientId(); }
QString TargetPlayer::name() const { return m_client->name(); }
QString TargetPlayer::character() const { return m_client->character(); }
int TargetPlayer::areaId() const { return m_client->areaId(); }
QString TargetPlayer::ipid() const { return m_client->ipid(); }
bool TargetPlayer::isAuthenticated() const { return m_client->isAuthenticated(); }

void TargetPlayer::reply(const QString &f_message)
{
    m_client->sendServerMessage(f_message);
}

bool TargetPlayer::hasSanction(const QString &f_sanction_id) const
{
    return m_client->hasSanction(f_sanction_id);
}

void TargetPlayer::setSanction(const QString &f_sanction_id, bool f_active)
{
    m_client->setSanction(f_sanction_id, f_active);
}

void TargetPlayer::changeArea(int f_area_id)
{
    m_client->changeArea(f_area_id);
}

void TargetPlayer::forceCharacterSelect()
{
    m_client->takeCharacter(-1, true);
}

void TargetPlayer::sendPacket(const QString &f_header, const QStringList &f_fields)
{
    m_client->sendPacket(f_header, f_fields);
}

void TargetPlayer::closeSocket()
{
    m_client->closeSocket();
}

QHostAddress TargetPlayer::remoteIp() const { return m_client->remoteIp(); }
QString TargetPlayer::hwid() const { return m_client->hwid(); }
QString TargetPlayer::characterName() const { return m_client->characterName(); }
bool TargetPlayer::isPmMuted() const { return m_client->isPmMuted(); }
void TargetPlayer::setTestimonySaving(bool f_state) { m_client->setTestimonySaving(f_state); }
QList<PlayerState *> TargetPlayer::players() const { return m_client->players; }
PlayerState *TargetPlayer::activePlayer() const { return m_client->active_player; }

// -- CommandContext --

CommandContext::CommandContext(akashi::ClientSession *f_invoker, ServerContext *f_server, QStringList f_arguments) :
    m_invoker(f_invoker),
    m_server(f_server),
    m_arguments(std::move(f_arguments))
{}

int CommandContext::clientId() const { return m_invoker->clientId(); }
QString CommandContext::name() const { return m_invoker->name(); }
QString CommandContext::character() const { return m_invoker->character(); }
QString CommandContext::characterName() const { return m_invoker->characterName(); }
int CommandContext::areaId() const { return m_invoker->areaId(); }

Area *CommandContext::area() const { return m_server->areaById(m_invoker->areaId()); }
QString CommandContext::areaName() const { return m_invoker->areaName(); }
QString CommandContext::ipid() const { return m_invoker->ipid(); }
QString CommandContext::hwid() const { return m_invoker->hwid(); }
bool CommandContext::isAuthenticated() const { return m_invoker->isAuthenticated(); }

bool CommandContext::canPerform(const QString &f_permission) const
{
    return m_invoker->canPerform(f_permission);
}

QString CommandContext::aclRoleId() const { return m_invoker->aclRoleId(); }
void CommandContext::setAclRoleId(const QString &f_role_id) { m_invoker->setAclRoleId(f_role_id); }
QString CommandContext::moderatorName() const { return m_invoker->moderatorName(); }
void CommandContext::setModeratorName(const QString &f_name) { m_invoker->setModeratorName(f_name); }
void CommandContext::setAuthenticated(bool f_state) { m_invoker->setAuthenticated(f_state); }
void CommandContext::setInLoginPrompt(bool f_state) { m_invoker->setInLoginPrompt(f_state); }

int CommandContext::argc() const { return m_arguments.size(); }
const QStringList &CommandContext::arguments() const { return m_arguments; }

QString CommandContext::argument(int f_index) const
{
    if (f_index >= 0 && f_index < m_arguments.size()) {
        return m_arguments.at(f_index);
    }
    return {};
}

std::optional<int> CommandContext::argumentAsInt(int f_index) const
{
    if (f_index >= 0 && f_index < m_arguments.size()) {
        bool l_ok = false;
        int l_value = m_arguments.at(f_index).toInt(&l_ok);
        if (l_ok) {
            return l_value;
        }
    }
    return std::nullopt;
}

void CommandContext::reply(const QString &f_message)
{
    m_invoker->sendServerMessage(f_message);
}

void CommandContext::replyToArea(const QString &f_message)
{
    m_invoker->sendServerMessageArea(f_message);
}

void CommandContext::replyToServer(const QString &f_message)
{
    m_invoker->sendServerBroadcast(f_message);
}

void CommandContext::sendPacket(const QString &f_header, const QStringList &f_fields)
{
    m_invoker->sendPacket(f_header, f_fields);
}

std::optional<TargetPlayer> CommandContext::resolveTarget(int f_argument_index)
{
    if (auto l_id = argumentAsInt(f_argument_index)) {
        akashi::ClientSession *l_client = m_server->clientById(*l_id);
        if (l_client) {
            return TargetPlayer(l_client);
        }
        reply("No client with ID " + m_arguments.at(f_argument_index) + " found.");
        return std::nullopt;
    }
    reply("That does not look like a valid ID.");
    return std::nullopt;
}

int CommandContext::genRand(int f_min, int f_max)
{
    return QRandomGenerator::system()->bounded(f_min, f_max + 1);
}

akashi::ServiceRegistry *CommandContext::services() const
{
    return m_server->services();
}

ServerContext *CommandContext::server() const
{
    return m_server;
}

} // namespace akashi
