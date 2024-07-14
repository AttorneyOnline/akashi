#include "playerstateobserver.h"

PlayerStateObserver::PlayerStateObserver(QObject *parent) :
    QObject{parent}
{}

PlayerStateObserver::~PlayerStateObserver() {}

void PlayerStateObserver::registerClient(AOClient *client)
{
    Q_ASSERT(!m_client_list.contains(client));

    PacketPR packet(client->clientId(), PacketPR::ADD);
    sendToClientList(packet);

    m_client_list.append(client);

    connect(client, &AOClient::nameChanged, this, &PlayerStateObserver::notifyNameChanged);
    connect(client, &AOClient::characterChanged, this, &PlayerStateObserver::notifyCharacterChanged);
    connect(client, &AOClient::characterNameChanged, this, &PlayerStateObserver::notifyCharacterNameChanged);
    connect(client, &AOClient::areaIdChanged, this, &PlayerStateObserver::notifyAreaIdChanged);

    QList<AOPacket *> packets;
    for (AOClient *i_client : qAsConst(m_client_list)) {
        packets.append(new PacketPR(i_client->clientId(), PacketPR::ADD));
        packets.append(new PacketPU(i_client->clientId(), PacketPU::NAME, i_client->name()));
        packets.append(new PacketPU(i_client->clientId(), PacketPU::CHARACTER, i_client->character()));
        packets.append(new PacketPU(i_client->clientId(), PacketPU::CHARACTER_NAME, i_client->characterName()));
        packets.append(new PacketPU(i_client->clientId(), PacketPU::AREA_ID, i_client->areaId()));
    }

    for (AOPacket *packet : qAsConst(packets)) {
        client->sendPacket(packet);
        delete packet;
    }
}

void PlayerStateObserver::unregisterClient(AOClient *client)
{
    Q_ASSERT(m_client_list.contains(client));

    disconnect(client, nullptr, this, nullptr);

    m_client_list.removeAll(client);

    PacketPR packet(client->clientId(), PacketPR::REMOVE);
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
    sendToClientList(PacketPU(qobject_cast<AOClient *>(sender())->clientId(), PacketPU::NAME, name));
}

void PlayerStateObserver::notifyCharacterChanged(const QString &character)
{
    sendToClientList(PacketPU(qobject_cast<AOClient *>(sender())->clientId(), PacketPU::CHARACTER, character));
}

void PlayerStateObserver::notifyCharacterNameChanged(const QString &characterName)
{
    sendToClientList(PacketPU(qobject_cast<AOClient *>(sender())->clientId(), PacketPU::CHARACTER_NAME, characterName));
}

void PlayerStateObserver::notifyAreaIdChanged(int areaId)
{
    sendToClientList(PacketPU(qobject_cast<AOClient *>(sender())->clientId(), PacketPU::AREA_ID, areaId));
}
