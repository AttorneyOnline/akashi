#pragma once

#include "akashidefs.h"
#include "aoclient.h"
#include "packet/packet_pr.h"

#include <QList>
#include <QObject>
#include <QString>

class PlayerStateObserver : public QObject
{
  public:
    explicit PlayerStateObserver(QObject *parent = nullptr);
    virtual ~PlayerStateObserver();

    void registerClient(AOClient *client);
    void unregisterClient(AOClient *client);

  private:
    QList<AOClient *> m_client_list;

    void sendToClientList(const AOPacket &packet);

  private Q_SLOTS:
    void notifyNameChanged(const QString &name);
    void notifyCharacterChanged(const QString &character);
    void notifyCharacterNameChanged(const QString &characterName);
    void notifyAreaIdChanged(int areaId);
};
