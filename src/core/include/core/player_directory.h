#pragma once

#include "akashi/service.h"
#include "akashi_core_export.h"

#include <QHash>
#include <QHostAddress>
#include <QString>
#include <QVector>

namespace akashi {

class ClientSession;

// The one place that knows who is connected under which ID. It owns the ID
// pool and the client list together, so they can never disagree - the old
// design kept three separate containers in sync by hand, with "free" encoded
// as a null pointer in a pre-filled map. Lookups here are null-safe by
// construction: a free or unknown ID gives nullptr, never a stale pointer,
// and the client list never contains nulls.
//
// Registered as the "akashi.players" service. ClientSession itself is not
// part of the plugin API; a plugin wraps each pointer in a TargetPlayer
// (core/command_context.h) to read names or send messages, and walks a
// session's characters through TargetPlayer::players(). The directory
// lives on the main thread and the sessions in it die on disconnect, so:
// query on the main thread, use the result right away, never store it.
class AKASHI_CORE_EXPORT PlayerDirectory : public IService
{
  public:
    QString serviceId() const override;
    ServiceVersion serviceVersion() const override;

    // How a free ID is picked for a new arrival. LastFreed hands out the
    // most recently freed ID; Lowest always picks the lowest free number.
    // Both give 0, 1, 2, ... on a fresh server.
    enum class IdAssignment
    {
        LastFreed,
        Lowest,
    };

    // Picks the assignment style once - when the server starts or the
    // setting changes - so takeId never re-decides it.
    void setIdAssignment(IdAssignment f_assignment);

    // Prepares f_capacity connection slots, with IDs 0 to f_capacity - 1.
    void setCapacity(int f_capacity);

    // True when no free ID is left.
    bool isFull() const;

    // Takes a free ID out of the pool, or -1 when full.
    int takeId();

    // Gives an unused ID back, for a connection rejected before it was added.
    void returnId(int f_id);

    // Files an accepted client under the ID it was given.
    void addClient(int f_id, akashi::ClientSession *f_client);

    // Drops the client filed under the ID and returns the ID to the pool.
    // Unknown IDs are ignored.
    void removeClient(int f_id);

    // The client with the given ID, or nullptr when that ID is free.
    akashi::ClientSession *clientById(int f_id) const;

    // Every connected client, oldest connection first.
    QVector<akashi::ClientSession *> clients() const;

    int clientCount() const;

    // How many connected clients share the given IP - an O(1) tally kept
    // as clients are added and removed, so the multiclient-limit check no
    // longer scans every client on each arrival. IPv4 and IPv4-mapped IPv6
    // count as the same origin (matching the old tolerant comparison).
    int sameIpCount(const QHostAddress &f_ip) const;

    // The tally key for an address, exposed so the grouping is testable.
    static QString ipKey(const QHostAddress &f_ip);

    // Forgets all clients and IDs without deleting anything; the caller
    // owns the objects. Used for the server's shutdown sequence.
    void clear();

  private:
    int takeLastFreedId();
    int takeLowestId();

    QVector<akashi::ClientSession *> m_clients;
    QHash<int, akashi::ClientSession *> m_clients_by_id;
    QVector<int> m_free_ids;         // most recently freed at the back
    QHash<QString, int> m_ip_counts; // live per-IP connection tally

    // The selected pick function; setIdAssignment replaces it.
    int (PlayerDirectory::*m_take_free_id)() = &PlayerDirectory::takeLastFreedId;
};

} // namespace akashi
