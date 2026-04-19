#ifndef CORE_CLIENT_SESSION_H
#define CORE_CLIENT_SESSION_H

#include "akashi_core_export.h"

#include <QHostAddress>
#include <QList>
#include <QSet>
#include <QString>

namespace akashi {

// One connection - the person behind it. A session owns 1..N PlayerStates
// (characters); everything here is per-person: identity, auth, the rate
// limiter, receive-preferences, and the moderation sanctions (keyed by
// ipid/hdid in the store, so they cover all the person's characters).
class AKASHI_CORE_EXPORT ClientSession
{
  public:
    // Transport and identity.
    int id = 0;
    QHostAddress remote_ip;
    QString hwid;
    QString ipid;
    bool joined = false;

    // Authentication.
    bool authenticated = false;
    QString acl_role_id;
    QString moderator_name;
    QString password;
    bool logging_in = false;

    // Idle tracking (the timer itself stays with the owner for now).
    bool afk = false;

    // Rate limiter.
    qint64 rate_limit_tick = 0;
    int packet_count = 0;

    // Active per-person punishments, keyed by id so plugins can add their own
    // without a new field. The core ids are the strings in the Sanction block
    // below; the persistent sanction store keys records by the same id ("kind").
    QSet<QString> sanctions;
    QList<int> charcurse_list; // the character ids a charcursed person may use

    bool hasSanction(const QString &f_id) const { return sanctions.contains(f_id); }
    void setSanction(const QString &f_id, bool f_active)
    {
        if (f_active) {
            sanctions.insert(f_id);
        }
        else {
            sanctions.remove(f_id);
        }
    }

    // Receive-preferences: what this person wants delivered to them.
    bool advert_enabled = true;
    bool pm_muted = false;
    bool global_enabled = true;
    QList<bool> casing_preferences = {false, false, false, false, false};

    // Misc person-level state.
    long last_wtce_time = 0;
    bool testimony_saving = false;
};

// The ids of the built-in sanctions. Plugins register their own alongside.
namespace Sanction {
inline const QString Mute = QStringLiteral("mute");
inline const QString OocMute = QStringLiteral("ooc_mute");
inline const QString DjBlock = QStringLiteral("dj_block");
inline const QString WtceBlock = QStringLiteral("wtce_block");
inline const QString Gimp = QStringLiteral("gimp");
inline const QString Shake = QStringLiteral("shake");
inline const QString Disemvowel = QStringLiteral("disemvowel");
inline const QString CharCurse = QStringLiteral("charcurse");
} // namespace Sanction

} // namespace akashi

#endif // CORE_CLIENT_SESSION_H
