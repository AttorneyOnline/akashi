#include "world/area.h"

#include "world/jukebox.h"

#include <QHash>
#include <QSettings>
#include <QTimer>

#include <algorithm>

namespace akashi {

Area::Area(int f_id, const QString &f_name, int f_floor_id, int f_x, QObject *parent) :
    QObject(parent),
    m_id(f_id),
    m_name(f_name),
    m_floor_id(f_floor_id),
    m_x(f_x)
{
    setupComponents();
}

Area::Area(const QString &f_settings_group, int f_id, int f_floor_id, int f_x,
           QSettings *f_areas_ini, QObject *parent) :
    QObject(parent),
    m_id(f_id),
    m_floor_id(f_floor_id),
    m_x(f_x)
{
    QStringList l_name_split = f_settings_group.split(":");
    l_name_split.removeFirst();
    m_name = l_name_split.join(":");
    if (m_name.isEmpty()) {
        m_name = QStringLiteral("Unnamed Area");
    }

    const auto l_access_from_string = [](const QString &f_text) {
        const QString l_text = f_text.toLower();
        if (l_text == QStringLiteral("mod"))
            return EvidenceStore::Access::Mod;
        if (l_text == QStringLiteral("cm"))
            return EvidenceStore::Access::Cm;
        if (l_text == QStringLiteral("hidden_cm") || l_text == QStringLiteral("hiddencm"))
            return EvidenceStore::Access::HiddenCm;
        return EvidenceStore::Access::FreeForAll;
    };

    f_areas_ini->beginGroup(f_settings_group);
    m_settings.background = f_areas_ini->value("background", "gs4").toString();
    m_settings.protected_area = f_areas_ini->value("protected_area", "false").toBool();
    m_settings.iniswap_allowed = f_areas_ini->value("iniswap_allowed", "true").toBool();
    m_settings.background_locked = f_areas_ini->value("bg_locked", "false").toBool();
    m_evidence_store.setAccess(l_access_from_string(f_areas_ini->value("evidence_mod", "ffa").toString()));
    m_settings.blankposting_allowed = f_areas_ini->value("blankposting_allowed", "true").toBool();
    m_settings.area_message = f_areas_ini->value("area_message").toString();
    m_settings.send_area_message_on_join = f_areas_ini->value("send_area_message_on_join", false).toBool();
    m_settings.force_immediate = f_areas_ini->value("force_immediate", "false").toBool();
    m_settings.music_allowed = f_areas_ini->value("toggle_music", "true").toBool();
    m_settings.shownames_allowed = f_areas_ini->value("shownames_allowed", "true").toBool();
    m_settings.ignore_background_list = f_areas_ini->value("ignore_bglist", "false").toBool();
    m_settings.jukebox_enabled = f_areas_ini->value("jukebox_enabled", "false").toBool();
    m_settings.play_command_enabled = f_areas_ini->value("playcmd_enabled", "false").toBool();
    m_settings.wtce_allowed = f_areas_ini->value("wtce_enabled", "true").toBool();
    m_settings.shouts_allowed = f_areas_ini->value("shouts_enabled", "true").toBool();
    f_areas_ini->endGroup();

    setupComponents();
}

void Area::setupComponents()
{
    for (int i = 0; i < 4; ++i) {
        m_timers.append(new QTimer(this));
    }
    m_jukebox = new Jukebox(this);
    connect(m_jukebox, &Jukebox::songStarted, this, &Area::onJukeboxSongStarted);
}

void Area::onJukeboxSongStarted(const JukeboxSong &f_song)
{
    m_jukebox->changeMusic(f_song.name, QStringLiteral("Jukebox"));
}

void Area::addPlayer(int f_player_id)
{
    if (m_players.contains(f_player_id)) {
        return;
    }
    m_players.append(f_player_id);
    Q_EMIT playerCountChanged(m_players.size());
}

void Area::removePlayer(int f_player_id)
{
    if (m_players.removeAll(f_player_id) > 0) {
        Q_EMIT playerCountChanged(m_players.size());
    }
}

void Area::addClient(int f_char_id, int f_player_id)
{
    if (f_char_id != -1) {
        takeCharacter(f_char_id);
    }
    addPlayer(f_player_id);
}

void Area::removeClient(int f_char_id, int f_player_id)
{
    if (f_char_id != -1) {
        releaseCharacter(f_char_id);
    }
    removePlayer(f_player_id);
    m_jukebox->playerLeft(f_player_id);
}

bool Area::takeCharacter(int f_char_id)
{
    if (m_characters_taken.contains(f_char_id)) {
        return false;
    }
    m_characters_taken.append(f_char_id);
    return true;
}

void Area::releaseCharacter(int f_char_id)
{
    m_characters_taken.removeAll(f_char_id);
}

bool Area::changeCharacter(int f_from, int f_to)
{
    if (m_characters_taken.contains(f_to)) {
        return false;
    }

    if (f_to != -1) {
        if (f_from != -1) {
            releaseCharacter(f_from);
        }
        takeCharacter(f_to);
        return true;
    }

    if (f_from != -1) {
        releaseCharacter(f_from);
    }

    return false;
}

void Area::addOwner(int f_player_id)
{
    if (!m_owners.contains(f_player_id)) {
        m_owners.append(f_player_id);
        Q_EMIT ownersChanged();
    }
    invite(f_player_id);
}

Area::OwnerRemoval Area::removeOwner(int f_player_id)
{
    // A non-owner has no spot to give up; their invitation stays too.
    if (!m_owners.contains(f_player_id)) {
        return OwnerRemoval::NotAnOwner;
    }
    m_owners.removeAll(f_player_id);
    Q_EMIT ownersChanged();
    uninvite(f_player_id);

    if (m_owners.isEmpty() && m_lock_state != LockState::Free) {
        setLockState(LockState::Free);
        return OwnerRemoval::RemovedAndUnlocked;
    }
    return OwnerRemoval::Removed;
}

bool Area::invite(int f_player_id)
{
    if (m_invited.contains(f_player_id)) {
        return false;
    }
    m_invited.append(f_player_id);
    return true;
}

bool Area::uninvite(int f_player_id)
{
    return m_invited.removeAll(f_player_id) > 0;
}

void Area::setLockState(LockState f_state)
{
    if (f_state != m_lock_state) {
        m_lock_state = f_state;
        Q_EMIT lockStateChanged(m_lock_state);
    }
}

void Area::setStatus(const QString &f_status)
{
    const QString l_status = f_status.left(30);
    if (l_status != m_status) {
        m_status = l_status;
        Q_EMIT statusChanged(m_status);
    }
}

std::optional<QString> Area::canonicalStatus(const QString &f_name)
{
    // The known names keep their exact old spellings on the wire.
    static const QHash<QString, QString> s_statuses = {
        {QStringLiteral("idle"), QStringLiteral("IDLE")},
        {QStringLiteral("rp"), QStringLiteral("RP")},
        {QStringLiteral("casing"), QStringLiteral("CASING")},
        {QStringLiteral("lfp"), QStringLiteral("LOOKING-FOR-PLAYERS")},
        {QStringLiteral("looking-for-players"), QStringLiteral("LOOKING-FOR-PLAYERS")},
        {QStringLiteral("recess"), QStringLiteral("RECESS")},
        {QStringLiteral("gaming"), QStringLiteral("GAMING")},
    };
    if (!s_statuses.contains(f_name)) {
        return std::nullopt;
    }
    return s_statuses[f_name];
}

bool Area::changeStatus(const QString &f_name)
{
    const std::optional<QString> l_status = canonicalStatus(f_name);
    if (!l_status) {
        return false;
    }
    setStatus(*l_status);
    return true;
}

int Area::evidenceIndexByVisibleIndex(int f_visible_index, const QString &f_client_pos, bool f_is_cm) const
{
    return m_evidence_store.itemIndexByVisibleIndex(f_visible_index, f_is_cm, f_client_pos);
}

int Area::visibleIndexByEvidenceIndex(int f_evidence_index, const QString &f_client_pos, bool f_is_cm) const
{
    return m_evidence_store.visibleIndexByItemIndex(f_evidence_index, f_is_cm, f_client_pos);
}

QList<Evidence> Area::visibleEvidence(bool f_can_see_hidden, const QString &f_side) const
{
    return m_evidence_store.visibleItems(f_can_see_hidden, f_side);
}

QString Area::currentMusic() const
{
    return m_jukebox->currentSong();
}

QString Area::currentAmbience() const
{
    return m_jukebox->currentAmbience();
}

QString Area::musicPlayerBy() const
{
    return m_jukebox->musicPlayedBy();
}

void Area::setMusicPlayedBy(const QString &f_music_player)
{
    m_jukebox->changeMusic(m_jukebox->currentSong(), f_music_player);
}

void Area::setCurrentMusic(const QString &f_current_song)
{
    m_jukebox->changeMusic(f_current_song, m_jukebox->musicPlayedBy());
}

void Area::changeMusic(const QString &f_source, const QString &f_new_song)
{
    m_jukebox->changeMusic(f_new_song, f_source);
}

void Area::changeAmbience(const QString &f_new_song)
{
    m_jukebox->changeAmbience(f_new_song);
}

void Area::changeHP(Side f_side, int f_new_hp)
{
    if (f_side == Side::DEFENCE) {
        m_def_hp = std::clamp(f_new_hp, 0, 10);
    }
    else if (f_side == Side::PROSECUTOR) {
        m_pro_hp = std::clamp(f_new_hp, 0, 10);
    }
}

void Area::appendJudgelog(const QString &f_new_log)
{
    if (m_judgelog.size() == 10) {
        m_judgelog.removeFirst();
    }
    m_judgelog.append(f_new_log);
}

bool Area::addNotecard(const QString &f_owner, const QString &f_notecard)
{
    m_notecards[f_owner] = f_notecard;

    if (f_notecard.isNull()) {
        m_notecards.remove(f_owner);
        return false;
    }
    return true;
}

QStringList Area::takeNotecards()
{
    QStringList l_notecards;
    for (auto l_it = m_notecards.constBegin(); l_it != m_notecards.constEnd(); ++l_it) {
        l_notecards << l_it.key() << ": " << l_it.value() << "\n";
    }

    m_notecards.clear();
    return l_notecards;
}

QString Area::areaMessage() const
{
    return m_settings.area_message.isEmpty() ? QStringLiteral("No area message set.") : m_settings.area_message;
}

void Area::toggleJukebox()
{
    m_settings.jukebox_enabled = !m_settings.jukebox_enabled;
    if (m_settings.jukebox_enabled) {
        m_jukebox->start();
    }
    else {
        m_jukebox->stop();
    }
}

} // namespace akashi
