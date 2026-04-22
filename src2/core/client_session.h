#ifndef CORE_CLIENT_SESSION_H
#define CORE_CLIENT_SESSION_H

#include "akashi_core_export.h"
#include "core/player_state.h"
#include "core/transport.h"
#include "proto/client_profile.h"
#include "proto/packet_codec.h"
#include "proto/packet_service.h"

#include <QHostAddress>
#include <QList>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QString>
#include <QTimer>

#include <memory>

namespace akashi {

// One connection - the person behind it. A session owns 1..N PlayerStates
// (characters); everything here is per-person: the transport, identity, auth,
// the rate limiter, receive-preferences, and the moderation sanctions (keyed
// by ipid/hdid in the store, so they cover all the person's characters).
class AKASHI_CORE_EXPORT ClientSession : public QObject
{
    Q_OBJECT

  public:
    // Takes ownership of the transport. The session outlives any one socket:
    // its signals are the stable seam the server wires against, so a
    // reconnect can bind a fresh transport to the same session.
    ClientSession(int f_id, ITransport *f_transport, QObject *parent = nullptr);

    // Sends a packet, or holds it while the wire is down so a returning
    // client gets it replayed. All outbound traffic goes through here.
    void write(const Packet &f_packet);

    // Adopts a new wire for this session, replacing and deleting the old one,
    // and replays whatever was held while no wire was open.
    void bindTransport(ITransport *f_transport);

    // Adds another playable character, refusing beyond f_limit per session.
    // The limit is the same multiclient_limit that caps connections per IP:
    // one cap on how many simultaneous presences a person gets, however
    // they are distributed.
    PlayerState *spawnPlayer(int f_id, int f_limit);

    // The characters this person plays. The constructor spawns the first one
    // (reusing the session id, so one-character clients are wire-identical);
    // only a richer protocol will ever spawn more. Owned as children.
    QList<PlayerState *> players;

    // The character the classic positional wire addresses - that wire is
    // strictly one-character, so for legacy clients this is the only entry.
    PlayerState *active_player = nullptr;

    // Transport and identity.
    ITransport *transport = nullptr;

    // Outbound packets held while the wire is down, replayed on rebind.
    // Bounded: when full the oldest is dropped and the overflow is recorded,
    // so a future resume can tell the replay is incomplete and force a resync.
    QQueue<Packet> pending_packets;
    bool pending_overflowed = false;
    int id = 0;
    QHostAddress remote_ip;
    QString hwid;
    QString ipid;
    bool identified = false; // has completed the ID handshake
    bool joined = false;

    // What the client told us about itself at the handshake (arch, version,
    // negotiated features). The client version lives here as profile.version.
    ClientProfile profile;

    // The server's packet pipeline and the codecs picked for this client,
    // refreshed when it identifies.
    std::shared_ptr<PacketService> packets;
    ResolvedCodecs codecs;

    // Authentication.
    bool authenticated = false;
    QString acl_role_id;
    QString moderator_name;
    QString password;
    bool logging_in = false;

    // Idle tracking.
    bool afk = false;
    QTimer *afk_timer = nullptr;

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

  Q_SIGNALS:
    // Forwarded from the owned transport, so receivers never touch the socket.
    void packetReceived(const Packet &f_packet);
    void transportClosed();
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
