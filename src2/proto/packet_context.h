#ifndef PROTO_PACKET_CONTEXT_H
#define PROTO_PACKET_CONTEXT_H

#include "akashi_core_export.h"
#include "proto/client_profile.h"
#include "proto/packet.h"

#include <QDateTime>
#include <QString>
#include <QStringList>

#include <optional>

namespace akashi {

// A ban entry as a handler needs it to tell the client.
struct BanRecord
{
    int id = -1;
    QString reason;
    QDateTime end;
    bool permanent = false;
};

// What a timer currently shows, for the TI packets sent on join.
struct TimerSnapshot
{
    bool running = false;
    int remaining_ms = 0;
};

// The client's area as the join sequence describes it.
struct AreaSnapshot
{
    int def_hp = 0;
    int pro_hp = 0;
    QString background;
    QString side;
    QList<TimerSnapshot> timers;
};

// Everything a packet handler may see and do. The connection implements it,
// so handlers never depend on the server's concrete classes.
class AKASHI_CORE_EXPORT IPacketContext
{
  public:
    virtual ~IPacketContext() = default;

    // The wire.
    virtual void sendPacket(const Packet &f_packet) = 0;
    virtual void sendServerMessage(const QString &f_message) = 0;
    virtual void closeConnection() = 0;

    // Who is connected.
    virtual int clientId() const = 0;
    virtual QString hwid() const = 0;
    virtual const ClientProfile &profile() const = 0;
    virtual bool isIdentified() const = 0;
    virtual bool hasJoined() const = 0;

    // Handshake steps, in the order a client performs them.
    virtual void setHwid(const QString &f_hwid) = 0;
    virtual void identify(const ClientProfile &f_profile) = 0;
    virtual void markJoined() = 0;
    virtual void finishJoin() = 0;

    // Admission bookkeeping.
    virtual void logConnectionAttempt() = 0;
    virtual std::optional<BanRecord> hardwareBan() const = 0;

    // World data the handshake hands out.
    virtual int playerCount() const = 0;
    virtual QStringList characters() const = 0;
    virtual QStringList areaNames() const = 0;
    virtual QStringList musicList() const = 0;
    virtual AreaSnapshot areaState() const = 0;
    virtual TimerSnapshot globalTimer() const = 0;

    // World changes handlers may trigger.
    virtual void announceCharsTaken() = 0;
    virtual void sendEvidenceList() = 0;
    virtual void sendFullArup() = 0;
    virtual void broadcastPlayerCount() = 0;
    virtual bool selectCharacter(int f_char_id) = 0;
};

} // namespace akashi

#endif // PROTO_PACKET_CONTEXT_H
