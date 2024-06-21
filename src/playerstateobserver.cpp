#include "playerstateobserver.h"

PlayerStateObserver::PlayerStateObserver(QObject *parent) :
    QObject{parent}
{}

PlayerStateObserver::~PlayerStateObserver() {}

void PlayerStateObserver::registerClient(AOClient *client)
{
    Q_ASSERT(!m_client_list.contains(client));

    PacketPLU packet(client->clientId(), PacketPLU::AddPlayerUpdate);
    sendToClientList(packet);

    m_client_list.append(client);

    connect(client, &AOClient::nameChanged, this, &PlayerStateObserver::notifyNameChanged);
    connect(client, &AOClient::characterChanged, this, &PlayerStateObserver::notifyCharacterChanged);
    connect(client, &AOClient::characterNameChanged, this, &PlayerStateObserver::notifyCharacterNameChanged);
    connect(client, &AOClient::areaIdChanged, this, &PlayerStateObserver::notifyAreaIdChanged);

    { // provide the player list to the new client
        QList<PacketPL::PlayerData> data_list;
        for (AOClient *i_client : qAsConst(m_client_list)) {
            PacketPL::PlayerData data;
            data.id = i_client->clientId();
            data.name = i_client->name();
            data.character = i_client->character();
            data.character_name = i_client->characterName();
            data.area_id = i_client->areaId();
            data_list.append(data);
        }

        PacketPL packet(data_list);
        client->sendPacket(&packet);
    }
}

void PlayerStateObserver::unregisterClient(AOClient *client)
{
    Q_ASSERT(m_client_list.contains(client));

    disconnect(client, nullptr, this, nullptr);

    m_client_list.removeAll(client);

    PacketPLU packet(client->clientId(), PacketPLU::RemovePlayerUpdate);
    sendToClientList(packet);
}

void PlayerStateObserver::sendToClientList(const AOPacket &packet)
{
    for (AOClient *client : qAsConst(m_client_list)) {
        client->sendPacket(&const_cast<AOPacket &>(packet));
    }
}

void PlayerStateObserver::notifyNameChanged(const QString &name)
{
    qDebug() << "PlayerStateObserver::notifyNameChanged" << qobject_cast<AOClient *>(sender())->clientId() << name;
    sendToClientList(PacketPU(qobject_cast<AOClient *>(sender())->clientId(), PacketPU::NameData, name));
}

void PlayerStateObserver::notifyCharacterChanged(const QString &character)
{
    qDebug() << "PlayerStateObserver::notifyCharacterChanged" << qobject_cast<AOClient *>(sender())->clientId() << character;
    sendToClientList(PacketPU(qobject_cast<AOClient *>(sender())->clientId(), PacketPU::CharacterData, character));
}

void PlayerStateObserver::notifyCharacterNameChanged(const QString &characterName)
{
    qDebug() << "PlayerStateObserver::notifyCharacterNameChanged" << qobject_cast<AOClient *>(sender())->clientId() << characterName;
    sendToClientList(PacketPU(qobject_cast<AOClient *>(sender())->clientId(), PacketPU::CharacterNameData, characterName));
}

void PlayerStateObserver::notifyAreaIdChanged(int areaId)
{
    qDebug() << "PlayerStateObserver::notifyAreaIdChanged" << qobject_cast<AOClient *>(sender())->clientId() << areaId;
    sendToClientList(PacketPU(qobject_cast<AOClient *>(sender())->clientId(), PacketPU::AreaIdData, areaId));
}
