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
    // its signals are the stable surface the server connects to, so a
    // reconnect can bind a fresh transport to the same session.
    ClientSession(int f_id, ITransport *f_transport, QObject *parent = nullptr);

    // Sends a packet, or holds it while the connection is down so a returning
    // client gets it replayed. All outgoing packets go through here.
    void write(const Packet &f_packet);

    // Adopts a new connection for this session, replacing and deleting the
    // old one, and sends whatever was held while no connection was open.
    void bindTransport(ITransport *f_transport);

    // Keeps the session alive after its connection was lost: the characters
    // stay visible and outgoing packets are held, until either a new
    // transport binds (the person came back) or the timer runs out and
    // reconnectTimedOut() tells the server to delete the session.
    void waitForReconnect(int f_grace_seconds);

    // Stops waiting, either because the person is back or because the server
    // needs the slot. bindTransport calls this when a new connection binds.
    void cancelReconnectWait();

    bool isWaitingForReconnect() const { return waiting_for_reconnect; }

    // Adds another playable character, refusing beyond f_limit per session.
    // The limit is the same multiclient_limit that caps connections per IP:
    // one cap on how many simultaneous presences a person gets, however
    // they are distributed.
    PlayerState *addPlayer(int f_id, int f_limit);

    // The characters this person plays. The constructor adds the first one
    // (reusing the session id, so nothing changes for one-character clients);
    // only a richer protocol will ever add more. Owned as children.
    QList<PlayerState *> players;

    // The character the classic protocol addresses - that protocol is
    // strictly one-character, so for legacy clients this is the only entry.
    PlayerState *active_player = nullptr;

    // Transport and identity.
    ITransport *transport = nullptr;

    // Outgoing packets held while the connection is down, sent once a new
    // one binds. Bounded: when full the oldest is dropped and the overflow
    // is recorded, so a future session-resume feature can tell the held
    // sequence is incomplete and send fresh state instead.
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

    // Set while the session survives a lost connection, bounded by the timer.
    bool waiting_for_reconnect = false;
    QTimer *reconnect_timer = nullptr;

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
    void transportClosed(DisconnectKind f_kind);

    // The reconnect wait ran out with nobody coming back; the server
    // deletes the session on this.
    void reconnectTimedOut();
};

// The ids of the built-in sanctions. Plugins register their own alongside.
namespace Sanction {
inline const QString Muted = QStringLiteral("muted");
inline const QString OocMuted = QStringLiteral("ooc_muted");
inline const QString DjBlocked = QStringLiteral("dj_blocked");
inline const QString WtceBlocked = QStringLiteral("wtce_blocked");
inline const QString Gimped = QStringLiteral("gimped");
inline const QString Shaken = QStringLiteral("shaken");
inline const QString Disemvoweled = QStringLiteral("disemvoweled");
inline const QString Medieval = QStringLiteral("medieval");
inline const QString CharCurse = QStringLiteral("charcurse");
} // namespace Sanction

} // namespace akashi

#endif // CORE_CLIENT_SESSION_H
