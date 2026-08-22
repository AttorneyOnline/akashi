#include "playerstateobserver.h"

PlayerStateObserver::PlayerStateObserver(QObject *parent) :
    QObject{parent}
{}

PlayerStateObserver::~PlayerStateObserver() {}

void PlayerStateObserver::registerClient(AOClient *client)
{
    Q_ASSERT(!m_client_list.contains(client));

    // First, add the new client to our list before broadcasting
    m_client_list.append(client);

    // Now connect signals from this client
    connect(client, &AOClient::nameChanged, this, &PlayerStateObserver::notifyNameChanged);
    connect(client, &AOClient::characterChanged, this, &PlayerStateObserver::notifyCharacterChanged);
    connect(client, &AOClient::characterNameChanged, this, &PlayerStateObserver::notifyCharacterNameChanged);
    connect(client, &AOClient::areaIdChanged, this, &PlayerStateObserver::notifyAreaIdChanged);

    // Notify all existing clients about the new player joining (including the new client itself now in the list)
    PacketPR packet(client->clientId(), PacketPR::ADD);
    sendToClientList(packet);

    // Send all existing client information to the new client
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
    // Create a copy of the list to safely iterate without issues from concurrent modifications
    // Also guards against clients that are being destroyed asynchronously
    QList<AOClient *> l_safe_list = m_client_list;
    
    for (AOClient *client : l_safe_list) {
        // Verify client is still in the list (not being unregistered concurrently)
        if (!m_client_list.contains(client)) {
            continue;
        }
        
        // Verify socket is still valid (QPointer becomes null when object is deleted)
        if (!client->m_socket) {
            continue;
        }
        
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
