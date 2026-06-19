#pragma once

#include "akashi_core_export.h"
#include "proto/ao2_protocol.h"
#include "proto/packet.h"

#include <QList>
#include <QObject>
#include <QString>

namespace akashi {
class PlayerState;
}

// Keeps every connected client's player list current. The observed unit is
// the PlayerState, not the connection: each character a person plays is its
// own PR/PU entry, keyed by the PlayerState id.
class AKASHI_CORE_EXPORT PlayerStateObserver : public QObject
{
  public:
    explicit PlayerStateObserver(QObject *parent = nullptr);
    virtual ~PlayerStateObserver();

    void registerPlayer(akashi::PlayerState *f_player);
    void unregisterPlayer(akashi::PlayerState *f_player);

  private:
    QList<akashi::PlayerState *> m_players;

    // Delivers one packet to every watching person once - a session with
    // several characters still receives a single copy.
    void sendToSessions(const akashi::Packet &f_packet);

  private Q_SLOTS:
    void notifyNameChanged(const QString &f_name);
    void notifyCharacterChanged(const QString &f_character);
    void notifyShownameChanged(const QString &f_showname);
    void notifyAreaIdChanged(int f_area_id);
};
