#include "core/player_state.h"

#include "core/client_session.h"

namespace akashi {

PlayerState::PlayerState(int f_id, ClientSession *f_session) :
    QObject(f_session),
    m_id(f_id),
    m_session(f_session)
{
}

void PlayerState::setOocName(const QString &f_name)
{
    if (f_name != m_ooc_name) {
        m_ooc_name = f_name;
        Q_EMIT nameChanged(m_ooc_name);
    }
}

void PlayerState::setCharacter(const QString &f_character)
{
    if (f_character != m_character) {
        m_character = f_character;
        Q_EMIT characterChanged(m_character);
    }
}

void PlayerState::setShowname(const QString &f_showname)
{
    if (f_showname != m_showname) {
        m_showname = f_showname;
        Q_EMIT shownameChanged(m_showname);
    }
}

void PlayerState::setAreaId(int f_area_id)
{
    if (f_area_id != m_area_id) {
        m_area_id = f_area_id;
        Q_EMIT areaIdChanged(m_area_id);
    }
}

} // namespace akashi
