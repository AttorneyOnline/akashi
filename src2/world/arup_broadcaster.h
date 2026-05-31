#pragma once

#include "akashi_core_export.h"
#include "world/area.h"

#include <QObject>
#include <QTimer>
#include <QVector>

#include <functional>

namespace akashi {

class Packet;

// Centralizes all four area-update (ARUP) broadcasts so every site that
// changes area state goes through Area setters (which emit signals) and
// this broadcaster reacts. No scattered arup() calls.
//
// Signals from Area are combined: multiple changes in one call stack
// produce a single broadcast per type at the end of the event-loop turn.
//
// Legacy clients receive the flat positional ARUP packet (one value per
// area in config order). When richer clients arrive, this is the
// place to add per-area incremental updates filtered by visibility zones
// (Area::visibleAreas) and tagged with area coordinates (x, floorId),
// so clients on large maps hear only about the rooms they can see.
class AKASHI_CORE_EXPORT ArupBroadcaster : public QObject
{
    Q_OBJECT

  public:
    enum class Type
    {
        PlayerCount = 0,
        Status = 1,
        Cm = 2,
        Lock = 3,
    };

    // Resolve a player id to "[id] character" for the CM column.
    // Returns empty string if the player is gone.
    using OwnerFormatter = std::function<QString(int)>;

    explicit ArupBroadcaster(QObject *parent = nullptr);

    void addArea(Area *area);
    void setOwnerFormatter(OwnerFormatter formatter);

    // Unicast all four ARUP types to one client (join time).
    void sendFullArup(int clientId);

    // Broadcast one ARUP type to everyone right now, bypassing the
    // deferred timer. The handshake handler uses this so the post-join
    // player-count update arrives in the same packet burst.
    void broadcastNow(Type type);

    Packet buildArup(Type type) const;

  Q_SIGNALS:
    void arupBroadcast(const akashi::Packet &packet);
    void arupUnicast(const akashi::Packet &packet, int clientId);

  private:
    void markDirty(Type type);
    void flush();
    QString formatOwners(const Area *area) const;
    static QString lockString(Area::LockState state);

    QVector<Area *> m_areas;
    OwnerFormatter m_format_owner;
    bool m_dirty[4] = {};
    QTimer m_flush_timer;
};

} // namespace akashi

