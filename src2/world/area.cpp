#include "world/area.h"

namespace akashi {

Area::Area(int f_id, const QString &f_name, int f_floor_id, int f_x, QObject *parent) :
    QObject(parent),
    m_id(f_id),
    m_name(f_name),
    m_floor_id(f_floor_id),
    m_x(f_x)
{
}

void Area::addPlayer(int f_player_id)
{
    if (m_players.contains(f_player_id)) {
        return;
    }
    m_players.append(f_player_id);
    Q_EMIT playerCountChanged(m_players.size());
}

void Area::removePlayer(int f_player_id)
{
    if (m_players.removeAll(f_player_id) > 0) {
        Q_EMIT playerCountChanged(m_players.size());
    }
}

bool Area::takeCharacter(int f_char_id)
{
    if (m_characters_taken.contains(f_char_id)) {
        return false;
    }
    m_characters_taken.append(f_char_id);
    return true;
}

void Area::releaseCharacter(int f_char_id)
{
    m_characters_taken.removeAll(f_char_id);
}

void Area::addOwner(int f_player_id)
{
    if (m_owners.contains(f_player_id)) {
        return;
    }
    m_owners.append(f_player_id);
    Q_EMIT ownersChanged();
}

bool Area::removeOwner(int f_player_id)
{
    if (m_owners.removeAll(f_player_id) == 0) {
        return false;
    }
    Q_EMIT ownersChanged();
    return true;
}

bool Area::invite(int f_player_id)
{
    if (m_invited.contains(f_player_id)) {
        return false;
    }
    m_invited.append(f_player_id);
    return true;
}

bool Area::uninvite(int f_player_id)
{
    return m_invited.removeAll(f_player_id) > 0;
}

void Area::setLockState(LockState f_state)
{
    if (f_state != m_lock_state) {
        m_lock_state = f_state;
        Q_EMIT lockStateChanged(m_lock_state);
    }
}

void Area::setStatus(Status f_status)
{
    if (f_status != m_status) {
        m_status = f_status;
        Q_EMIT statusChanged(m_status);
    }
}

} // namespace akashi
