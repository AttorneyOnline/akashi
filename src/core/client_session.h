#pragma once

#include "akashi_core_export.h"
#include "core/player_state.h"
#include "proto/client_profile.h"
#include "proto/packet_codec.h"
#include "proto/packet_context.h"
#include "proto/packet_service.h"
#include "proto/transport.h"
#include "world/area.h"

#include <QFutureWatcher>
#include <QHostAddress>
#include <QList>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QString>
#include <QTimer>

#include <memory>
#include <optional>

class ServerContext;

namespace akashi {

class TextFilterRegistry;
struct PacketSpec;

// One connection - the person behind it. A session owns 1..N PlayerStates
// (characters) and everything per-person: the transport, identity, auth,
// the rate limiter, receive-preferences, and the moderation sanctions. It
// is also the IPacketContext the packet handlers talk to, and it carries
// the game verbs a person performs: joining areas, chatting, presenting
// evidence, logging in.
class AKASHI_CORE_EXPORT ClientSession : public QObject, public IPacketContext
{
    Q_OBJECT

  public:
    // How far this connection has come: a fresh socket, past the handshake,
    // or a full player in the world. The stage only ever moves forward.
    enum class SessionStage
    {
        Connected,
        Identified,
        Joined,
    };

    // Takes ownership of the transport. The session outlives any one socket:
    // its signals are the stable surface the server connects to, so a
    // reconnect can bind a fresh transport to the same session.
    ClientSession(ServerContext *f_server, ITransport *f_transport, int f_id, QObject *parent = nullptr);

    // Runs leave() as a safety net; the server normally calls it explicitly.
    ~ClientSession() override;

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
    PlayerState *addPlayer(int f_id, int f_limit);

    // The characters this person plays. The constructor adds the first one
    // (reusing the session id); only a richer protocol will ever add more.
    QList<PlayerState *> players;

    // The character the classic protocol addresses.
    PlayerState *active_player = nullptr;

    ITransport *transport = nullptr;

    // Outgoing packets held while the connection is down, sent once a new
    // one binds. Bounded: when full the oldest is dropped and the overflow
    // is recorded so a resume can tell the held sequence is incomplete.
    QQueue<Packet> pending_packets;
    bool pending_overflowed = false;
    int id = 0;
    QHostAddress remote_ip;
    QString session_hwid;
    QString session_ipid;

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

    // Active per-person punishments, keyed by id so plugins can add their
    // own without a new field.
    QSet<QString> sanctions;
    QList<int> charcurse_list;

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

    // The character id AO uses for "no character": a spectator.
    const int SPECTATOR_ID = -1;

    // --- Person-level accessors ---

    QString ipid() const;
    SessionStage stage() const;

    // Moves the lifecycle forward; a lower or equal stage is a no-op.
    void advanceStage(SessionStage f_stage);
    bool isJoined() const;
    bool isAuthenticated() const override;
    void calculateIpid();
    ServerContext *server();
    int clientId() const;

    QString name() const;
    void setName(const QString &f_name);
    QString character() const;
    void setCharacter(const QString &f_character);
    QString characterName() const;
    void setCharacterName(const QString &f_showname);
    int areaId() const;
    void setAreaId(const int f_area_id);

    bool isSpectator() const;
    void setSpectator(bool f_spectator);

    // --- Game verbs ---

    void sendServerMessage(const QString &message) override;
    void sendServerMessageArea(QString message);
    void sendServerBroadcast(QString message);
    void sendEvidenceList(Area *area) const;
    void updateEvidenceList(Area *area);
    QString dezalgo(QString p_text);
    bool canModifyEvidence(Area *area);
    bool canModifyEvidence();
    void sendFullArup();
    // The one door every character change goes through: person gates, the
    // character_changed before-rules, the area's uniqueness mechanism, the
    // PV/CharsCheck sends and the after-rule dispatch. Returns the refusal
    // reason on failure - empty for the silent refusals the mechanism has
    // always given. f_announce_done sends the DONE that pushes the client
    // back to the character select screen on a switch to spectator.
    std::optional<QString> takeCharacter(int f_char_id, bool f_announce_done = false);
    void loginAttempt(QString message);
    void changeArea(int new_area) override;
    int floorCount() const override;
    int floorAreaId(int f_floor_id, int f_x) const override;
    QStringList floorAreaNames() const override;
    int floorAreaToGlobal(int f_local_index) const override;
    std::optional<QString> checkBeforeRule(const QString &f_event, const QVariantMap &f_payload = {}) override;
    QVariantMap runTransformRules(const QString &f_event, const QVariantMap &f_payload) override;
    void runAfterRule(const QString &f_event, const QVariantMap &f_payload = {}) override;
    void handleCommand(QString command, int argc, QStringList argv);
    QString decodeMessage(QString incoming_message);
    void addStatement(QStringList packet);
    QStringList updateStatement(QStringList packet);
    void updateJudgeLog(Area *area, ClientSession *client, QString action);

    // --- The IPacketContext surface the packet handlers use ---

    void closeConnection() override;
    QString hwid() const override;
    const ClientProfile &profile() const override;
    bool isIdentified() const override;
    void setHwid(const QString &f_hwid) override;
    void identify(const ClientProfile &f_profile) override;
    void markJoined() override;
    void finishJoin() override;
    void logConnectionAttempt() override;
    std::optional<BanRecord> hardwareBan() const override;
    QString serverNickname() const override;
    int maxMessageLength() const override;
    bool webaoEnabled() const override;
    int maxPlayerCount() const override;
    QString serverDescription() const override;
    QUrl assetUrl() const override;
    QString motd() const override;
    int playerCount() const override;
    QStringList characters() const override;
    QStringList musicList() const override;
    TimerSnapshot globalTimer() const override;
    void announceCharsTaken() override;
    void sendEvidenceList() override;
    void broadcastPlayerCount() override;
    bool selectCharacter(int f_char_id) override;
    bool canUseOocChat() const override;
    QString oocName() const override;
    void setOocName(const QString &f_name) override;
    bool isInLoginPrompt() const override;
    void attemptLogin(const QString &f_message) override;
    void runCommand(const QString &f_command, const QStringList &f_arguments) override;
    void broadcastOoc(const QString &f_message) override;
    int evidenceCount() const override;
    void deleteEvidence(int f_index) override;
    void replaceEvidence(int f_index, const QString &f_name, const QString &f_description, const QString &f_image) override;
    void setCasingPreferences(const QList<bool> &f_preferences) override;
    bool canUseIcChat() const override;
    int characterId() const override;
    bool isFirstPerson() const override;
    void setIniswap(const QString &f_character) override;
    void setEmote(const QString &f_emote) override;
    void setOffset(const QString &f_offset) override;
    void setFlipping(const QString &f_flipping) override;
    QString iniswap() const;
    QString emote() const;
    QString offset() const;
    QString flipping() const;
    QString pos() const;
    int pairingWith() const;
    void setPairingWith(int f_char_id) override;
    QString lastIcMessage() const override;
    void setLastIcMessage(const QString &f_message) override;
    void updatePosition(const QString &f_position) override;
    std::optional<QString> applyTextFilters(const QString &f_text, akashi::TextChannel f_channel) const override;

    bool isAfk() const;
    void setAfk(bool f_afk);
    bool isPmMuted() const;
    void setPmMuted(bool f_pm_muted);
    bool isAdvertEnabled() const;
    void setAdvertEnabled(bool f_advert_enabled);
    bool isCharCursed() const;
    void setCharCursed(bool f_char_cursed);
    bool isTestimonySaving() const;
    void setTestimonySaving(bool f_testimony_saving);

    QHostAddress remoteIp() const;
    QString moderatorName() const;
    void setModeratorName(const QString &f_name);
    QString aclRoleId() const;
    void setAclRoleId(const QString &f_role_id);
    void setAuthenticated(bool f_state);

    void setInLoginPrompt(bool f_in_login_prompt);
    void setFirstPerson(bool f_first_person);
    bool isGlobalEnabled() const;
    void setGlobalEnabled(bool f_global_enabled);
    QList<bool> casingPreferences() const;
    QList<int> charCurseList() const;
    void addCharCurse(int f_char_id);
    void clearCharCurse();
    void changePosition(QString f_new_pos);
    void closeSocket();
    bool isIcMessageAllowed() const override;
    bool canActInArea() override;
    bool isImmediateForced() const override;
    QString areaSide() const override;
    QStringList lastAreaMessage() const override;
    PairInfo resolvePair(int f_pair_id) override;
    QStringList applyTestimony(const QStringList &f_fields) override;
    void broadcastIc(const QStringList &f_fields, int f_evidence_index) override;
    bool hasSong(const QString &f_name) const override;
    bool isDjBlocked() const override;
    std::optional<QString> playMusic(const QString &f_song, MusicSource f_source, const QString &f_char_id, const QString &f_effects) override;
    // The ambience verb; commands are its only doors, so it lives off the
    // packet context. The source is PlayCommand or Background.
    std::optional<QString> playAmbience(const QString &f_song, MusicSource f_source);

    // The scene verbs below are command-only doors like playAmbience: each
    // owns its rules, the mutation and the broadcasts, and returns the
    // refusal reason or nothing when done.

    // Background: rules, the list validation, the BN broadcast, the
    // background's ambience through playAmbience, and the area notice.
    std::optional<QString> changeBackground(const QString &f_background);

    // The background side, gated by the same background rules.
    std::optional<QString> changeBackgroundSide(const QString &f_side);

    // Status: validates against the known names, then rules, the commit
    // and the area notice. The area-update broadcast follows the change.
    std::optional<QString> changeAreaStatus(const QString &f_name);

    // The case document; empty text resets it to "No document.".
    std::optional<QString> changeDocument(const QString &f_text);

    // The area lock; locking or spectating invites everyone present.
    std::optional<QString> setAreaLock(Area::LockState f_state);

    // Case manager spots. The ownership checks stay with the commands.
    std::optional<QString> addAreaOwner(int f_target);
    std::optional<QString> removeAreaOwner(int f_target);
    bool isWtceBlocked() const override;
    bool startWtceCooldown() override;
    std::optional<QString> useWtce(const QString &f_splash, const std::optional<QString> &f_variant) override;
    std::optional<QString> changePenalty(int f_bar, int f_value) override;
    void addEvidence(const QString &f_name, const QString &f_description, const QString &f_image) override;
    void broadcastArea(const Packet &f_packet) override;
    void broadcastCaseAlert(const QList<bool> &f_needs, const Packet &f_packet) override;
    void setCharacterPassword(const QString &f_password) override;
    bool canPerform(const QString &f_permission) const override;
    QString areaName() const override;
    std::optional<QString> playerName(int f_client_id) const override;
    void broadcastModerators(const Packet &f_packet) override;
    void recordModcall(const QString &f_reason) override;
    void kickPlayer(int f_client_id, const QString &f_reason) override;
    void banPlayer(int f_client_id, int f_duration, const QString &f_reason) override;

  public Q_SLOTS:
    // Handles an incoming packet, checking authorisation and argument count.
    void handlePacket(const Packet &packet);

    // Withdraws the person's presence from the server: area roster, ARUP
    // counts, invites and CM spots. Idempotent, and runs while the session
    // is alive - never from the destructor doing real work.
    void leave();

    void sendPacket(const Packet &packet) override;
    void sendPacket(QString header, QStringList contents);
    void sendPacket(QString header);

    void onAfkTimeout();

  Q_SIGNALS:
    // The person completed the join handshake.
    void joined();

    // Forwarded from the owned transport, so receivers never touch the socket.
    void packetReceived(const Packet &f_packet);
    void transportClosed(DisconnectKind f_kind);

    // The reconnect wait ran out with nobody coming back; the server
    // deletes the session on this.
    void reconnectTimedOut();

  private Q_SLOTS:
    // Finishes the advanced login once the hash worker returns; loginAttempt
    // binds the attempt's particulars ahead of the watcher's finished signal.
    void onLoginHashComputed(QFutureWatcher<QString> *f_watcher, const QString &f_username, const QString &f_password, const QString &f_stored_hash, const QString &f_acl_role, bool f_needs_rehash);

  private:
    // The person's profile as told at the handshake; profile() exposes it.
    ClientProfile m_profile;

    // Where the connection stands in its lifecycle; advanceStage moves it.
    SessionStage m_stage = SessionStage::Connected;

    // Set once leave() has run, making it idempotent.
    bool m_left = false;

    // The session's active character - the one the classic protocol
    // addresses.
    PlayerState *player() const { return active_player; }

    ServerContext *m_server;

    // Runs a packet through its registered handler after the usual checks.
    void handleRegisteredPacket(const Packet &f_packet, const PacketSpec &f_spec);

    // Marks the person active again; any packet except the keepalive counts.
    void resetAfk(const QString &f_header);

    bool checkTestimonySymbols(const QString &message);
};

} // namespace akashi
