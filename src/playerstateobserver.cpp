#include "playerstateobserver.h"

#include "core/client_session.h"
#include "core/player_state.h"

#include <QSet>

PlayerStateObserver::PlayerStateObserver(QObject *parent) :
    QObject{parent}
{}

PlayerStateObserver::~PlayerStateObserver() {}

void PlayerStateObserver::registerPlayer(akashi::PlayerState *f_player)
{
    Q_ASSERT(!m_players.contains(f_player));

    sendToSessions(akashi::Packet("PR", {QString::number(f_player->id()), QString::number(ao2::PLAYER_LIST_ADD)}));

    m_players.append(f_player);

    connect(f_player, &akashi::PlayerState::nameChanged, this, &PlayerStateObserver::notifyNameChanged);
    connect(f_player, &akashi::PlayerState::characterChanged, this, &PlayerStateObserver::notifyCharacterChanged);
    connect(f_player, &akashi::PlayerState::shownameChanged, this, &PlayerStateObserver::notifyShownameChanged);
    connect(f_player, &akashi::PlayerState::areaIdChanged, this, &PlayerStateObserver::notifyAreaIdChanged);

    // Catch the newcomer up on the whole roster, their own entry included.
    for (akashi::PlayerState *i_player : qAsConst(m_players)) {
        const QString l_id = QString::number(i_player->id());
        f_player->session()->write(akashi::Packet("PR", {l_id, QString::number(ao2::PLAYER_LIST_ADD)}));
        f_player->session()->write(akashi::Packet("PU", {l_id, QString::number(ao2::PLAYER_DATA_NAME), i_player->oocName()}));
        f_player->session()->write(akashi::Packet("PU", {l_id, QString::number(ao2::PLAYER_DATA_CHARACTER), i_player->character()}));
        f_player->session()->write(akashi::Packet("PU", {l_id, QString::number(ao2::PLAYER_DATA_CHARACTER_NAME), i_player->showname()}));
        f_player->session()->write(akashi::Packet("PU", {l_id, QString::number(ao2::PLAYER_DATA_AREA_ID), QString::number(i_player->areaId())}));
    }
}

void PlayerStateObserver::unregisterPlayer(akashi::PlayerState *f_player)
{
    // Players that never joined are not registered, so there is nothing to do.
    if (!m_players.contains(f_player)) {
        return;
    }

    disconnect(f_player, nullptr, this, nullptr);

    m_players.removeAll(f_player);

    sendToSessions(akashi::Packet("PR", {QString::number(f_player->id()), QString::number(ao2::PLAYER_LIST_REMOVE)}));
}

void PlayerStateObserver::sendToSessions(const akashi::Packet &f_packet)
{
    QSet<akashi::ClientSession *> l_sent;
    for (akashi::PlayerState *i_player : qAsConst(m_players)) {
        akashi::ClientSession *l_session = i_player->session();
        if (!l_sent.contains(l_session)) {
            l_sent.insert(l_session);
            l_session->write(f_packet);
        }
    }
}

void PlayerStateObserver::notifyNameChanged(const QString &f_name)
{
    sendToSessions(akashi::Packet("PU", {QString::number(qobject_cast<akashi::PlayerState *>(sender())->id()), QString::number(ao2::PLAYER_DATA_NAME), f_name}));
}

void PlayerStateObserver::notifyCharacterChanged(const QString &f_character)
{
    sendToSessions(akashi::Packet("PU", {QString::number(qobject_cast<akashi::PlayerState *>(sender())->id()), QString::number(ao2::PLAYER_DATA_CHARACTER), f_character}));
}

void PlayerStateObserver::notifyShownameChanged(const QString &f_showname)
{
    sendToSessions(akashi::Packet("PU", {QString::number(qobject_cast<akashi::PlayerState *>(sender())->id()), QString::number(ao2::PLAYER_DATA_CHARACTER_NAME), f_showname}));
}

void PlayerStateObserver::notifyAreaIdChanged(int f_area_id)
{
    sendToSessions(akashi::Packet("PU", {QString::number(qobject_cast<akashi::PlayerState *>(sender())->id()), QString::number(ao2::PLAYER_DATA_AREA_ID), QString::number(f_area_id)}));
}
