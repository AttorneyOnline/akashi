#pragma once

#include "akashi/area_rule.h"
#include "akashi_core_export.h"
#include "world/area_settings.h"
#include "world/evidence_store.h"
#include "world/floodguard.h"
#include "world/testimony_recorder.h"

#include <QList>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

#include <optional>

class QSettings;
class QTimer;

namespace akashi {

class Jukebox;
struct JukeboxSong;

// One area of the world: identity, its place on the map, who is in it, the
// moderation state the area list shows, and the pieces a scene is made of -
// evidence, testimony, jukebox, timers, settings and the rules applied here.
// Every core mutation goes through a setter that reports actual changes, so
// the area-update broadcaster can subscribe instead of being called by hand.
//
// The map is a grid: y is the floor (hub), x the area's position on it -
// the same coordinates the GA packet moves by. The area never builds
// packets; it emits semantic signals and answers queries.
class AKASHI_CORE_EXPORT Area : public QObject
{
    Q_OBJECT

  public:
    enum class LockState
    {
        Free,
        Spectatable,
        Locked,
    };
    Q_ENUM(LockState)

    enum class Side
    {
        DEFENCE,
        PROSECUTOR,
    };

    Area(int f_id, const QString &f_name, int f_floor_id, int f_x, QObject *parent = nullptr);

    // The server's constructor: the group is the "N:Name" section of the
    // settings file the area reads itself from.
    Area(const QString &f_settings_group, int f_id, int f_floor_id, int f_x,
         QSettings *f_areas_ini, QObject *parent = nullptr);

    // Introspectable area state. Anything a plugin (native or scripted) may
    // read or tune is a property here - adding one exposes it everywhere with
    // no code elsewhere, and only the WRITE ones can be set from a plugin.
    Q_PROPERTY(QString name READ name)
    Q_PROPERTY(int player_count READ playerCount)
    Q_PROPERTY(QString background READ background)
    Q_PROPERTY(bool is_protected READ isProtected)
    Q_PROPERTY(QString evidence_mod READ evidenceModName)
    Q_PROPERTY(QString status READ status WRITE setStatus)
    Q_PROPERTY(QString lock_state READ lockStateName WRITE setLockStateName)
    Q_PROPERTY(bool music_allowed READ isMusicAllowed WRITE setMusicAllowed)
    Q_PROPERTY(bool shouts_allowed READ isShoutAllowed WRITE setShoutAllowed)
    Q_PROPERTY(bool wtce_allowed READ isWtceAllowed WRITE setWtceAllowed)
    Q_PROPERTY(bool blankposting_allowed READ isBlankpostingAllowed WRITE setBlankpostingAllowed)
    Q_PROPERTY(bool iniswap_allowed READ isIniswapAllowed WRITE setIniswapAllowed)

    // --- Identity and place on the map ---

    int id() const { return m_id; }
    QString name() const { return m_name; }
    void setName(const QString &f_name) { m_name = f_name; }
    QString displayName() const { return "[" + QString::number(m_id) + "] " + m_name; }
    int floorId() const { return m_floor_id; }
    int x() const { return m_x; }

    // Removing an area compacts the id space; the server renumbers the
    // surviving areas with this.
    void renumber(int f_id, int f_floor_id, int f_x)
    {
        m_id = f_id;
        m_floor_id = f_floor_id;
        m_x = f_x;
    }

    // Which areas a player standing here has in their area list, so big maps
    // don't show hundreds of entries. Empty means every area on the floor -
    // the default, and today's behavior.
    QSet<int> visibleAreas() const { return m_visible_areas; }
    void setVisibleAreas(const QSet<int> &f_area_ids) { m_visible_areas = f_area_ids; }

    // --- Membership, keyed by player id ---

    QVector<int> players() const { return m_players; }
    int playerCount() const { return m_players.size(); }
    void addPlayer(int f_player_id);
    void removePlayer(int f_player_id);

    // Files a player into the area together with their character; the
    // jukebox learns about departures for its vote bookkeeping.
    void addClient(int f_char_id, int f_player_id);
    void removeClient(int f_char_id, int f_player_id);

    // --- Characters in use here, in take order ---

    QList<int> charactersTaken() const { return m_characters_taken; }
    bool takeCharacter(int f_char_id);
    void releaseCharacter(int f_char_id);

    // Trades one character for another; false when the wish is taken.
    bool changeCharacter(int f_from, int f_to);

    // --- Case managers and invitations ---

    QList<int> owners() const { return m_owners; }
    bool hasOwners() const { return !m_owners.isEmpty(); }

    // Becoming an owner includes the invitation.
    void addOwner(int f_player_id);

    // How removeOwner answered: a non-owner id is refused and changes
    // nothing; RemovedAndUnlocked means the area also freed itself because
    // its last owner left.
    enum class OwnerRemoval
    {
        NotAnOwner,
        Removed,
        RemovedAndUnlocked,
    };

    // An owner gives up the spot, invitation included.
    OwnerRemoval removeOwner(int f_player_id);

    QList<int> invited() const { return m_invited; }
    bool invite(int f_player_id);
    bool uninvite(int f_player_id);

    LockState lockState() const { return m_lock_state; }
    void setLockState(LockState f_state);
    void lock() { setLockState(LockState::Locked); }
    void unlock() { setLockState(LockState::Free); }
    void spectatable() { setLockState(LockState::Spectatable); }

    // --- The status line the area list shows ---
    // The protocol never limited it to fixed values - the well-known ones
    // (IDLE, CASING, ...) are convention - so any text works; anything past
    // 30 characters is cut off.

    QString status() const { return m_status; }
    void setStatus(const QString &f_status);

    // The stored spelling of a known status name, or nothing when the name
    // is unknown. The validation query the status verb checks before it
    // dispatches rules and commits.
    static std::optional<QString> canonicalStatus(const QString &f_name);

    // Validates through canonicalStatus and stores the old spelling.
    // False when the name is unknown.
    bool changeStatus(const QString &f_name);

    // --- Evidence ---

    QList<Evidence> evidence() const { return m_evidence_store.items(); }
    void appendEvidence(const Evidence &f_evidence) { m_evidence_store.append(f_evidence); }
    void deleteEvidence(int f_evidence_id) { m_evidence_store.remove(f_evidence_id); }
    void replaceEvidence(int f_evidence_id, const Evidence &f_evidence) { m_evidence_store.replace(f_evidence_id, f_evidence); }
    void swapEvidence(int f_evidence_id1, int f_evidence_id2) { m_evidence_store.swap(f_evidence_id1, f_evidence_id2); }
    void setEvidenceOwnerToAll(int f_evidence_id) { m_evidence_store.revealToAll(f_evidence_id); }
    EvidenceStore::Access evidenceAccess() const { return m_evidence_store.access(); }
    void setEvidenceAccess(EvidenceStore::Access f_access) { m_evidence_store.setAccess(f_access); }
    int evidenceIndexByVisibleIndex(int f_visible_index, const QString &f_client_pos, bool f_is_cm) const;
    int visibleIndexByEvidenceIndex(int f_evidence_index, const QString &f_client_pos, bool f_is_cm) const;
    QList<Evidence> visibleEvidence(bool f_can_see_hidden, const QString &f_side) const;
    QString taggedEvidenceDescription(const QString &f_description) const { return m_evidence_store.taggedDescription(f_description); }

    // --- Testimony ---

    TestimonyRecorder *testimonyRecorder() { return &m_testimony_recorder; }

    // --- Music ---

    Jukebox *jukebox() const { return m_jukebox; }
    QString currentMusic() const;
    QString currentAmbience() const;
    QString musicPlayerBy() const;
    void setMusicPlayedBy(const QString &f_music_player);
    void setCurrentMusic(const QString &f_current_song);
    void changeMusic(const QString &f_source, const QString &f_new_song);
    void changeAmbience(const QString &f_new_song);

    // --- Timers and the IC floodguard ---

    QList<QTimer *> timers() const { return m_timers; }
    bool isMessageAllowed() const { return m_floodguard.isMessageAllowed(); }
    void startMessageFloodguard(int f_duration) { m_floodguard.start(f_duration); }

    // --- The courtroom scene ---

    int defHP() const { return m_def_hp; }
    int proHP() const { return m_pro_hp; }
    void changeHP(Side f_side, int f_new_hp);

    QString document() const { return m_document; }
    void changeDoc(const QString &f_new_doc) { m_document = f_new_doc; }

    QString side() const { return m_side; }
    void setSide(const QString &f_side) { m_side = f_side; }

    QString background() const { return m_settings.background; }
    void setBackground(const QString &f_background) { m_settings.background = f_background; }

    QStringList judgelog() const { return m_judgelog; }
    void appendJudgelog(const QString &f_new_log);

    const QStringList &lastICMessage() const { return m_last_ic_message; }
    void updateLastICMessage(const QStringList &f_last_message) { m_last_ic_message = f_last_message; }

    bool addNotecard(const QString &f_owner, const QString &f_notecard);
    QStringList notecards();

    // --- Owner-tunable settings ---

    bool isProtected() const { return m_settings.protected_area; }
    bool isBlankpostingAllowed() const { return m_settings.blankposting_allowed; }
    void toggleBlankposting() { m_settings.blankposting_allowed = !m_settings.blankposting_allowed; }
    bool isIniswapAllowed() const { return m_settings.iniswap_allowed; }
    void toggleIniswap() { m_settings.iniswap_allowed = !m_settings.iniswap_allowed; }
    bool isBgLocked() const { return m_settings.background_locked; }
    void toggleBgLock() { m_settings.background_locked = !m_settings.background_locked; }
    bool isShownameAllowed() const { return m_settings.shownames_allowed; }
    bool ignoreBgList() const { return m_settings.ignore_background_list; }
    void toggleIgnoreBgList() { m_settings.ignore_background_list = !m_settings.ignore_background_list; }
    bool isMusicAllowed() const { return m_settings.music_allowed; }
    void toggleMusic() { m_settings.music_allowed = !m_settings.music_allowed; }
    bool forceImmediate() const { return m_settings.force_immediate; }
    void toggleImmediate() { m_settings.force_immediate = !m_settings.force_immediate; }
    bool isWtceAllowed() const { return m_settings.wtce_allowed; }
    void toggleWtceAllowed() { m_settings.wtce_allowed = !m_settings.wtce_allowed; }
    bool isShoutAllowed() const { return m_settings.shouts_allowed; }
    void toggleShoutAllowed() { m_settings.shouts_allowed = !m_settings.shouts_allowed; }
    bool isMedievalMode() const { return m_settings.medieval_mode; }
    void toggleMedievalMode() { m_settings.medieval_mode = !m_settings.medieval_mode; }
    bool isJukeboxEnabled() const { return m_settings.jukebox_enabled; }
    void toggleJukebox();
    bool isPlayEnabled() const { return m_settings.play_command_enabled; }
    QString areaMessage() const;
    void changeAreaMessage(const QString &f_new_message) { m_settings.area_message = f_new_message; }
    void clearAreaMessage() { m_settings.area_message.clear(); }
    bool sendAreaMessageOnJoin() const { return m_settings.send_area_message_on_join; }
    void toggleAreaMessageJoin() { m_settings.send_area_message_on_join = !m_settings.send_area_message_on_join; }

    // --- Property adapters ---
    // Plain void setters and string views of the enum settings, so the
    // Q_PROPERTY block above can expose the whole tunable surface uniformly.

    void setMusicAllowed(bool f_on) { m_settings.music_allowed = f_on; }
    void setShoutAllowed(bool f_on) { m_settings.shouts_allowed = f_on; }
    void setWtceAllowed(bool f_on) { m_settings.wtce_allowed = f_on; }
    void setBlankpostingAllowed(bool f_on) { m_settings.blankposting_allowed = f_on; }
    void setIniswapAllowed(bool f_on) { m_settings.iniswap_allowed = f_on; }

    QString evidenceModName() const
    {
        switch (m_evidence_store.access()) {
        case EvidenceStore::Access::FreeForAll:
            return QStringLiteral("ffa");
        case EvidenceStore::Access::Mod:
            return QStringLiteral("mod");
        case EvidenceStore::Access::Cm:
            return QStringLiteral("cm");
        case EvidenceStore::Access::HiddenCm:
            return QStringLiteral("hidden_cm");
        }
        return QStringLiteral("ffa");
    }

    QString lockStateName() const
    {
        switch (m_lock_state) {
        case LockState::Free:
            return QStringLiteral("free");
        case LockState::Spectatable:
            return QStringLiteral("spectatable");
        case LockState::Locked:
            return QStringLiteral("locked");
        }
        return QStringLiteral("free");
    }

    void setLockStateName(const QString &f_name)
    {
        if (f_name == QStringLiteral("locked"))
            setLockState(LockState::Locked);
        else if (f_name == QStringLiteral("spectatable"))
            setLockState(LockState::Spectatable);
        else
            setLockState(LockState::Free);
    }

    // --- Rules applied to this area ---

    QVector<BeforeRuleEntry> &beforeRules() { return m_before_rules; }
    QVector<AfterRuleEntry> &afterRules() { return m_after_rules; }
    QVector<TransformRuleEntry> &transformRules() { return m_transform_rules; }
    const QVector<BeforeRuleEntry> &beforeRules() const { return m_before_rules; }
    const QVector<AfterRuleEntry> &afterRules() const { return m_after_rules; }
    const QVector<TransformRuleEntry> &transformRules() const { return m_transform_rules; }

  Q_SIGNALS:
    // One signal per area-update type the protocol knows; the broadcaster
    // subscribes to exactly these four.
    void playerCountChanged(int f_count);
    void statusChanged(const QString &f_status);
    void ownersChanged();
    void lockStateChanged(LockState f_state);

  private Q_SLOTS:
    // A song the jukebox starts on its own becomes the area's current music.
    void onJukeboxSongStarted(const JukeboxSong &f_song);

  private:
    void setupComponents();

    int m_id;
    QString m_name;
    int m_floor_id;
    int m_x;
    QSet<int> m_visible_areas;
    QVector<int> m_players;
    QList<int> m_characters_taken;
    QList<int> m_owners;
    QList<int> m_invited;
    LockState m_lock_state = LockState::Free;
    QString m_status = QStringLiteral("IDLE");

    AreaSettings m_settings;
    EvidenceStore m_evidence_store;
    TestimonyRecorder m_testimony_recorder;
    Jukebox *m_jukebox = nullptr;

    QList<QTimer *> m_timers;
    Floodguard m_floodguard;

    QString m_side;
    QString m_document = QStringLiteral("No document.");
    int m_def_hp = 10;
    int m_pro_hp = 10;
    QStringList m_judgelog;
    QStringList m_last_ic_message;
    QMap<QString, QString> m_notecards;

    QVector<BeforeRuleEntry> m_before_rules;
    QVector<AfterRuleEntry> m_after_rules;
    QVector<TransformRuleEntry> m_transform_rules;
};

} // namespace akashi
