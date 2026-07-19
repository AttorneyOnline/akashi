#pragma once

#include "akashi/service.h"

#include <QDateTime>
#include <QList>
#include <QString>

namespace akashi {

// A past ban as the moderation history hands it out.
struct BanHistoryEntry
{
    int ban_id = -1;
    QString ipid;
    QString hdid;
    qint64 time = 0;     // epoch seconds when issued
    qint64 duration = 0; // seconds; -2 is permanent
    QString reason;
    QString moderator;
};

// An active sanction row from the shared store.
struct SanctionEntry
{
    QString ipid;
    QString sanction;
    QString issuer;
    qint64 issued = 0;  // epoch seconds
    qint64 expires = 0; // epoch seconds; -1 holds until lifted by hand
    QString hwid;       // matched besides the ipid; may be empty
};

// The service-mediated door to the moderation data: plugins read history
// and place timed sanctions here instead of reaching into the database.
// Bans stay read-only; the sanction store is the one surface human
// moderators and automated moderation both write, so a bot's mute looks
// and lifts exactly like /mute. Registered as "akashi.moderation".
// Main thread only.
class ModerationService : public IService
{
  public:
    QString serviceId() const override { return QStringLiteral("akashi.moderation"); }
    ServiceVersion serviceVersion() const override { return {1, 0, 0}; }

    // Every ban ever issued against the identifier.
    virtual QList<BanHistoryEntry> banHistory(const QString &f_ipid) const = 0;
    virtual QList<BanHistoryEntry> banHistoryByHwid(const QString &f_hwid) const = 0;

    // The identifier's sanctions still in force - timed ones that have
    // not expired, and untimed ones waiting for a hand lift.
    virtual QList<SanctionEntry> activeSanctions(const QString &f_ipid) const = 0;

    // True when the named ACL role holds the permission. This is how a
    // session-less actor (a moderation bot) answers to the same roles
    // file as human moderators: the owner grants or withholds its powers
    // by editing the role it is configured to wear.
    virtual bool roleCanPerform(const QString &f_role_id, const QString &f_permission) const = 0;

    // Applies a sanction to every online client with the ipid, stores it,
    // and schedules the lift - the same door /mute uses.
    virtual void applyTimedSanction(const QString &f_ipid, const QString &f_sanction_id, const QDateTime &f_until, const QString &f_issuer) = 0;

    // Lifts a sanction now: session flags, the stored row and the
    // scheduled lift all go.
    virtual void liftSanction(const QString &f_ipid, const QString &f_sanction_id) = 0;
};

} // namespace akashi
