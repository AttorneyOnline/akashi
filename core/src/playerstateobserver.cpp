#include "include/playerstateobserver.h"
#include "include/aoclient.h"
#include "include/server.h"

PlayerStateObserver::PlayerStateObserver(QObject *parent)
    : QObject{parent}
{}

void PlayerStateObserver::registerClient(AOClient *client)
{
    observed_clients.insert(client->clientId(), client);
    connect(client, &AOClient::displaynameChanged, this, &PlayerStateObserver::displayNameChanged);
    connect(client, &AOClient::characterChanged, this, &PlayerStateObserver::characterChanged);
}

void PlayerStateObserver::deregisterClient(int id)
{
    AOClient *l_client = observed_clients.take(id);
    disconnect(l_client, nullptr, this, nullptr);
}

void PlayerStateObserver::playerChangedArea(AreaMoveType f_type, int area_id)
{
    AOClient *l_client = qobject_cast<AOClient *>(sender());
    if (l_client) {
        qDebug() << "Player with uid" << l_client->clientId() << "has changed their area";
    }
}

void PlayerStateObserver::displayNameChanged()
{
    AOClient *l_client = qobject_cast<AOClient *>(sender());
    if (l_client) {
        qDebug() << "Player with uid" << l_client->clientId() << "has changed their displayname to"
                 << l_client->m_showname;
    }
}

void PlayerStateObserver::characterChanged()
{
    AOClient *l_client = qobject_cast<AOClient *>(sender());
    if (l_client) {
        qDebug() << "Player with uid" << l_client->clientId() << "has changed their character to"
                 << l_client->currentCharacter();
    }
}
