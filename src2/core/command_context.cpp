#include "core/command_context.h"

#include "aoclient.h"
#include "server.h"

namespace akashi {

// -- TargetPlayer --

TargetPlayer::TargetPlayer(AOClient *f_client) :
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
    if (f_sanction_id == sanction::muted) return m_client->isMuted();
    if (f_sanction_id == sanction::ooc_muted) return m_client->isOocMuted();
    if (f_sanction_id == sanction::dj_blocked) return m_client->isDjBlocked();
    if (f_sanction_id == sanction::wtce_blocked) return m_client->isWtceBlocked();
    return false;
}

void TargetPlayer::setSanction(const QString &f_sanction_id, bool f_active)
{
    if (f_sanction_id == sanction::muted) m_client->setMuted(f_active);
    else if (f_sanction_id == sanction::ooc_muted) m_client->setOocMuted(f_active);
    else if (f_sanction_id == sanction::dj_blocked) m_client->setDjBlocked(f_active);
    else if (f_sanction_id == sanction::wtce_blocked) m_client->setWtceBlocked(f_active);
}

void TargetPlayer::changeArea(int f_area_id)
{
    m_client->changeArea(f_area_id);
}

void TargetPlayer::forceCharacterSelect()
{
    m_client->changeCharacter(-1);
    m_client->sendPacket("DONE");
}

// -- CommandContext --

CommandContext::CommandContext(AOClient *f_invoker, Server *f_server, QStringList f_arguments) :
    m_invoker(f_invoker),
    m_server(f_server),
    m_arguments(std::move(f_arguments))
{}

int CommandContext::clientId() const { return m_invoker->clientId(); }
QString CommandContext::name() const { return m_invoker->name(); }
QString CommandContext::character() const { return m_invoker->character(); }
int CommandContext::areaId() const { return m_invoker->areaId(); }
QString CommandContext::areaName() const { return m_invoker->areaName(); }
QString CommandContext::ipid() const { return m_invoker->ipid(); }
QString CommandContext::hwid() const { return m_invoker->hwid(); }
bool CommandContext::isAuthenticated() const { return m_invoker->isAuthenticated(); }

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

std::optional<TargetPlayer> CommandContext::resolveTarget(int f_argument_index)
{
    if (auto l_id = argumentAsInt(f_argument_index)) {
        AOClient *l_client = m_server->clientById(*l_id);
        if (l_client) {
            return TargetPlayer(l_client);
        }
        reply("No client with ID " + m_arguments.at(f_argument_index) + " found.");
        return std::nullopt;
    }
    reply("That does not look like a valid ID.");
    return std::nullopt;
}

akashi::ServiceRegistry *CommandContext::services() const
{
    return m_server->services();
}

Server *CommandContext::server() const
{
    return m_server;
}

} // namespace akashi
