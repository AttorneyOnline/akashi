#include "playerstateobserver.h"

#include "aoclient.h"

PlayerStateObserver::PlayerStateObserver(QObject *parent) :
    QObject{parent}
{}

PlayerStateObserver::~PlayerStateObserver() {}

void PlayerStateObserver::registerClient(AOClient *client)
{
    Q_ASSERT(!m_client_list.contains(client));

    sendToClientList(akashi::Packet("PR", {QString::number(client->clientId()), QString::number(ao2::PLAYER_LIST_ADD)}));

    m_client_list.append(client);

    connect(client, &AOClient::nameChanged, this, &PlayerStateObserver::notifyNameChanged);
    connect(client, &AOClient::characterChanged, this, &PlayerStateObserver::notifyCharacterChanged);
    connect(client, &AOClient::characterNameChanged, this, &PlayerStateObserver::notifyCharacterNameChanged);
    connect(client, &AOClient::areaIdChanged, this, &PlayerStateObserver::notifyAreaIdChanged);

    for (AOClient *i_client : qAsConst(m_client_list)) {
        const QString l_id = QString::number(i_client->clientId());
        client->sendPacket(akashi::Packet("PR", {l_id, QString::number(ao2::PLAYER_LIST_ADD)}));
        client->sendPacket(akashi::Packet("PU", {l_id, QString::number(ao2::PLAYER_DATA_NAME), i_client->name()}));
        client->sendPacket(akashi::Packet("PU", {l_id, QString::number(ao2::PLAYER_DATA_CHARACTER), i_client->character()}));
        client->sendPacket(akashi::Packet("PU", {l_id, QString::number(ao2::PLAYER_DATA_CHARACTER_NAME), i_client->characterName()}));
        client->sendPacket(akashi::Packet("PU", {l_id, QString::number(ao2::PLAYER_DATA_AREA_ID), QString::number(i_client->areaId())}));
    }
}

void PlayerStateObserver::unregisterClient(AOClient *client)
{
    // Clients that never joined are not registered, so there is nothing to do.
    if (!m_client_list.contains(client)) {
        return;
    }

    disconnect(client, nullptr, this, nullptr);

    m_client_list.removeAll(client);

    sendToClientList(akashi::Packet("PR", {QString::number(client->clientId()), QString::number(ao2::PLAYER_LIST_REMOVE)}));
}

void PlayerStateObserver::sendToClientList(const akashi::Packet &packet)
{
    for (AOClient *client : qAsConst(m_client_list)) {
        client->sendPacket(packet);
    }
}

void PlayerStateObserver::notifyNameChanged(const QString &name)
{
    sendToClientList(akashi::Packet("PU", {QString::number(qobject_cast<AOClient *>(sender())->clientId()), QString::number(ao2::PLAYER_DATA_NAME), name}));
}

void PlayerStateObserver::notifyCharacterChanged(const QString &character)
{
    sendToClientList(akashi::Packet("PU", {QString::number(qobject_cast<AOClient *>(sender())->clientId()), QString::number(ao2::PLAYER_DATA_CHARACTER), character}));
}

void PlayerStateObserver::notifyCharacterNameChanged(const QString &characterName)
{
    sendToClientList(akashi::Packet("PU", {QString::number(qobject_cast<AOClient *>(sender())->clientId()), QString::number(ao2::PLAYER_DATA_CHARACTER_NAME), characterName}));
}

void PlayerStateObserver::notifyAreaIdChanged(int areaId)
{
    sendToClientList(akashi::Packet("PU", {QString::number(qobject_cast<AOClient *>(sender())->clientId()), QString::number(ao2::PLAYER_DATA_AREA_ID), QString::number(areaId)}));
}
