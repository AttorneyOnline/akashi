#include "core/player_directory.h"

#include "client_session.h"

#include <algorithm>
#include <functional>

namespace akashi {

QString PlayerDirectory::serviceId() const
{
    return QStringLiteral("akashi.players");
}

ServiceVersion PlayerDirectory::serviceVersion() const
{
    return {1, 1, 0};
}

void PlayerDirectory::setCapacity(int f_capacity)
{
    clear();
    for (int i = f_capacity - 1; i >= 0; i--) {
        m_free_ids.append(i);
    }
}

bool PlayerDirectory::isFull() const
{
    return m_free_ids.isEmpty();
}

void PlayerDirectory::setIdAssignment(IdAssignment f_assignment)
{
    m_take_free_id = f_assignment == IdAssignment::Lowest
                         ? &PlayerDirectory::takeLowestId
                         : &PlayerDirectory::takeLastFreedId;
}

int PlayerDirectory::takeId()
{
    if (m_free_ids.isEmpty()) {
        return -1;
    }
    return std::invoke(m_take_free_id, this);
}

int PlayerDirectory::takeLastFreedId()
{
    return m_free_ids.takeLast();
}

int PlayerDirectory::takeLowestId()
{
    const auto l_lowest = std::min_element(m_free_ids.cbegin(), m_free_ids.cend());
    return m_free_ids.takeAt(l_lowest - m_free_ids.cbegin());
}

void PlayerDirectory::returnId(int f_id)
{
    m_free_ids.append(f_id);
}

void PlayerDirectory::addClient(int f_id, akashi::ClientSession *f_client)
{
    Q_ASSERT(!m_clients_by_id.contains(f_id));
    m_clients_by_id.insert(f_id, f_client);
    m_clients.append(f_client);
    ++m_ip_counts[ipKey(f_client->remoteIp())];
}

void PlayerDirectory::removeClient(int f_id)
{
    akashi::ClientSession *l_client = m_clients_by_id.take(f_id);
    if (!l_client) {
        return;
    }
    m_clients.removeAll(l_client);
    m_free_ids.append(f_id);
    const auto l_it = m_ip_counts.find(ipKey(l_client->remoteIp()));
    if (l_it != m_ip_counts.end() && --l_it.value() <= 0) {
        m_ip_counts.erase(l_it);
    }
}

int PlayerDirectory::sameIpCount(const QHostAddress &f_ip) const
{
    return m_ip_counts.value(ipKey(f_ip));
}

QString PlayerDirectory::ipKey(const QHostAddress &f_ip)
{
    // IPv4 and IPv4-mapped IPv6 collapse to one key so the same origin
    // counts once, matching the old isEqual tolerant comparison; a real
    // IPv6 address keys by its own text.
    bool l_is_v4 = false;
    const quint32 l_v4 = f_ip.toIPv4Address(&l_is_v4);
    return l_is_v4 ? QStringLiteral("v4:") + QString::number(l_v4)
                   : QStringLiteral("v6:") + f_ip.toString();
}

akashi::ClientSession *PlayerDirectory::clientById(int f_id) const
{
    return m_clients_by_id.value(f_id);
}

QVector<akashi::ClientSession *> PlayerDirectory::clients() const
{
    return m_clients;
}

int PlayerDirectory::clientCount() const
{
    return m_clients.size();
}

void PlayerDirectory::clear()
{
    m_clients.clear();
    m_clients_by_id.clear();
    m_free_ids.clear();
    m_ip_counts.clear();
}

} // namespace akashi
