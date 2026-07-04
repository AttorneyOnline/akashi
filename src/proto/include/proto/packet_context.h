#pragma once

#include "akashi_core_export.h"
#include "proto/client_profile.h"
#include "proto/packet.h"

#include <QDateTime>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>

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

// A pair partner's visible state, resolved for an in-character message.
struct PairInfo
{
    QString name = "0";
    QString emote = "0";
    QString offset = "0";
    QString flip = "0";
    bool paired = false;
};

// Everything a packet handler may see and do. The connection implements it,
// so handlers never depend on the server's concrete classes.
class AKASHI_CORE_EXPORT IPacketContext
{
  public:
    virtual ~IPacketContext() = default;

    // The network.
    virtual void sendPacket(const Packet &f_packet) = 0;
    virtual void sendServerMessage(const QString &f_message) = 0;
    virtual void closeConnection() = 0;

    // Who is connected.
    virtual int clientId() const = 0;
    virtual QString hwid() const = 0;
    virtual const ClientProfile &profile() const = 0;
    virtual bool isIdentified() const = 0;
    virtual bool isJoined() const = 0;

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
    virtual QStringList musicList() const = 0;
    virtual TimerSnapshot globalTimer() const = 0;

    // World changes handlers may trigger.
    virtual void announceCharsTaken() = 0;
    virtual void sendEvidenceList() = 0;
    virtual void broadcastPlayerCount() = 0;
    virtual bool selectCharacter(int f_char_id) = 0;

    // Out-of-character chat.
    virtual bool canUseOocChat() const = 0;
    virtual QString oocName() const = 0;
    virtual void setOocName(const QString &f_name) = 0;
    virtual bool isInLoginPrompt() const = 0;
    virtual void attemptLogin(const QString &f_message) = 0;
    virtual void runCommand(const QString &f_command, const QStringList &f_arguments) = 0;
    virtual void broadcastOoc(const QString &f_message) = 0;

    // Evidence in the client's area.
    virtual bool isEvidenceHiddenCm() const = 0;
    virtual int evidenceCount() const = 0;
    virtual void deleteEvidence(int f_index) = 0;
    virtual void replaceEvidence(int f_index, const QString &f_name, const QString &f_description, const QString &f_image) = 0;

    // Casing alerts.
    virtual void setCasingPreferences(const QList<bool> &f_preferences) = 0;

    // In-character chat: who is speaking.
    virtual bool canUseIcChat() const = 0;
    virtual bool isSpectator() const = 0;
    virtual QString character() const = 0;
    virtual QString characterName() const = 0;
    virtual void setCharacterName(const QString &f_showname) = 0;
    virtual int characterId() const = 0;
    virtual bool isFirstPerson() const = 0;
    virtual void setIniswap(const QString &f_character) = 0;
    virtual void setEmote(const QString &f_emote) = 0;
    virtual void setOffset(const QString &f_offset) = 0;
    virtual void setFlipping(const QString &f_flipping) = 0;
    virtual QString lastIcMessage() const = 0;
    virtual void setLastIcMessage(const QString &f_message) = 0;
    virtual void updatePosition(const QString &f_position) = 0;
    // Runs the invoker's active text filters; no value means the message drops.
    virtual std::optional<QString> applyTextFilters(const QString &f_text) const = 0;

    // In-character chat: the area's rules and state.
    virtual bool isIcMessageAllowed() const = 0;
    virtual bool canActInArea() = 0;
    virtual bool isShoutAllowed() const = 0;
    virtual bool isShownameAllowed() const = 0;
    virtual bool isImmediateForced() const = 0;
    virtual QString areaSide() const = 0;
    virtual QStringList lastAreaMessage() const = 0;

    // In-character chat: doing things with the finished message. The evidence
    // index is the sender's own, since testimony playback may swap the fields.
    virtual PairInfo resolvePair(int f_pair_id) = 0;
    virtual QStringList applyTestimony(const QStringList &f_fields) = 0;
    virtual void broadcastIc(const QStringList &f_fields, int f_evidence_index) = 0;

    // Music.
    virtual bool hasSong(const QString &f_name) const = 0;
    virtual bool isDjBlocked() const = 0;
    virtual bool isJukeboxEnabled() const = 0;
    virtual QString queueJukeboxSong(const QString &f_song) = 0;
    virtual QString resolveSongAlias(const QString &f_song) = 0;
    virtual void recordMusicChange(const QString &f_song) = 0;

    // Judge controls.
    virtual bool isWtceBlocked() const = 0;
    virtual bool isWtceAllowed() const = 0;
    // True when the judge controls are off cooldown; stamps the new use.
    virtual bool startWtceCooldown() = 0;
    virtual void logJudgeAction(const QString &f_action) = 0;

    // Areas.
    virtual void changeArea(int f_area_index) = 0;
    virtual int floorCount() const = 0;
    virtual int floorAreaId(int f_floor_id, int f_x) const = 0;

    // The area names visible to this client (only their floor).
    virtual QStringList floorAreaNames() const = 0;
    // Maps a floor-local area index to the global area id.
    virtual int floorAreaToGlobal(int f_local_index) const = 0;

    // General rule dispatch against the client's current area and floor.
    // checkBeforeRule returns the block reason, or nullopt if allowed.
    virtual std::optional<QString> checkBeforeRule(const QString &f_event, const QVariantMap &f_payload = {}) = 0;
    virtual void runAfterRule(const QString &f_event, const QVariantMap &f_payload = {}) = 0;

    // Evidence creation; deletion and edits are further up.
    virtual void addEvidence(const QString &f_name, const QString &f_description, const QString &f_image) = 0;

    // Sends one finished packet to everyone in the area.
    virtual void broadcastArea(const Packet &f_packet) = 0;

    // Judge penalty bars; the bar is 1 for defence, 2 for prosecution.
    virtual void setPenalty(int f_bar, int f_value) = 0;
    virtual int penalty(int f_bar) const = 0;

    // Casing: sends the alert to every client whose preferences match a need.
    virtual void broadcastCaseAlert(const QList<bool> &f_needs, const Packet &f_packet) = 0;

    // The password a client wants to unlock a protected character with.
    virtual void setCharacterPassword(const QString &f_password) = 0;

    // Configuration values proto handlers need.
    virtual QString serverNickname() const = 0;
    virtual int maxMessageLength() const = 0;
    virtual QStringList wordFilters() const = 0;
    virtual bool webaoEnabled() const = 0;
    virtual int maxPlayerCount() const = 0;
    virtual QString serverDescription() const = 0;
    virtual QUrl assetUrl() const = 0;
    virtual QString motd() const = 0;
    // Moderation.
    virtual bool isAuthenticated() const = 0;
    virtual bool canPerform(const QString &f_permission) const = 0;
    virtual QString areaName() const = 0;
    // The OOC name of another connected player; empty-but-present names stay.
    virtual std::optional<QString> playerName(int f_client_id) const = 0;
    virtual void broadcastModerators(const Packet &f_packet) = 0;
    virtual void recordModcall(const QString &f_reason) = 0;
    virtual void kickPlayer(int f_client_id, const QString &f_reason) = 0;
    virtual void banPlayer(int f_client_id, int f_duration, const QString &f_reason) = 0;
};

} // namespace akashi
