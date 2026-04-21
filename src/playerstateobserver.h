#pragma once

#include "akashi_core_export.h"
#include "proto/ao2_protocol.h"
#include "proto/packet.h"

#include <QList>
#include <QObject>
#include <QString>

class AOClient;

class AKASHI_CORE_EXPORT PlayerStateObserver : public QObject
{
  public:
    explicit PlayerStateObserver(QObject *parent = nullptr);
    virtual ~PlayerStateObserver();

    void registerClient(AOClient *client);
    void unregisterClient(AOClient *client);

  private:
    QList<AOClient *> m_client_list;

    void sendToClientList(const akashi::Packet &packet);

  private Q_SLOTS:
    void notifyNameChanged(const QString &name);
    void notifyCharacterChanged(const QString &character);
    void notifyCharacterNameChanged(const QString &characterName);
    void notifyAreaIdChanged(int areaId);
};
