//////////////////////////////////////////////////////////////////////////////////////
//    akashi - a server for Attorney Online 2                                       //
//    Copyright (C) 2020  scatterflower                                             //
//                                                                                  //
//    This program is free software: you can redistribute it and/or modify          //
//    it under the terms of the GNU Affero General Public License as                //
//    published by the Free Software Foundation, either version 3 of the            //
//    License, or (at your option) any later version.                               //
//                                                                                  //
//    This program is distributed in the hope that it will be useful,               //
//    but WITHOUT ANY WARRANTY; without even the implied warranty of                //
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                 //
//    GNU Affero General Public License for more details.                           //
//                                                                                  //
//    You should have received a copy of the GNU Affero General Public License      //
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.        //
//////////////////////////////////////////////////////////////////////////////////////

#include "area_data.h"

#include "world/area.h"

#include "config_manager.h"
#include "music_manager.h"
#include "proto/packet.h"

#include <QRegularExpression>

#include <algorithm>

AreaData::AreaData(QString p_name, int p_index, MusicManager *p_music_manager = nullptr) :
    m_music_manager(p_music_manager),
    m_document("No document."),
    m_area_message("No area message set."),
    m_defHP(10),
    m_proHP(10),
    m_currentMusic("~stop.mp3"),
    m_statement(0),
    m_judgelog(),
    m_lastICMessage(),
    m_send_area_message(false),
    m_can_send_wtce(true),
    m_can_use_shouts(true)
{
    QStringList name_split = p_name.split(":");
    name_split.removeFirst();
    QString l_name = name_split.join(":");
    if (l_name.isEmpty()) {
        l_name = "Unnamed Area";
    }
    // The core state lives in the world model; this class delegates to it
    // until the remaining pieces move over. Until floor layouts load from
    // config, every area sits on the one default floor at its index.
    m_area = new akashi::Area(p_index, l_name, 0, p_index, this);
    m_display_name = "[" + QString::number(p_index) + "] " + l_name;
    QSettings *areas_ini = ConfigManager::areaData();
    areas_ini->beginGroup(p_name);
    m_background = areas_ini->value("background", "gs4").toString();
    m_isProtected = areas_ini->value("protected_area", "false").toBool();
    m_iniswapAllowed = areas_ini->value("iniswap_allowed", "true").toBool();
    m_bgLocked = areas_ini->value("bg_locked", "false").toBool();
    m_eviMod = QVariant(areas_ini->value("evidence_mod", "FFA").toString().toUpper()).value<EvidenceMod>();
    m_blankpostingAllowed = areas_ini->value("blankposting_allowed", "true").toBool();
    m_area_message = areas_ini->value("area_message").toString();
    m_send_area_message = areas_ini->value("send_area_message_on_join", false).toBool();
    m_forceImmediate = areas_ini->value("force_immediate", "false").toBool();
    m_toggleMusic = areas_ini->value("toggle_music", "true").toBool();
    m_shownameAllowed = areas_ini->value("shownames_allowed", "true").toBool();
    m_ignoreBgList = areas_ini->value("ignore_bglist", "false").toBool();
    m_jukebox = areas_ini->value("jukebox_enabled", "false").toBool();
    m_playcmd = areas_ini->value("playcmd_enabled", "false").toBool();
    m_can_send_wtce = areas_ini->value("wtce_enabled", "true").toBool();
    m_can_use_shouts = areas_ini->value("shouts_enabled", "true").toBool();
    areas_ini->endGroup();
    QTimer *timer1 = new QTimer();
    m_timers.append(timer1);
    QTimer *timer2 = new QTimer();
    m_timers.append(timer2);
    QTimer *timer3 = new QTimer();
    m_timers.append(timer3);
    QTimer *timer4 = new QTimer();
    m_timers.append(timer4);
    m_jukebox_timer = new QTimer();
    connect(m_jukebox_timer, &QTimer::timeout,
            this, &AreaData::switchJukeboxSong);
    m_message_floodguard_timer = new QTimer(this);
    connect(m_message_floodguard_timer, &QTimer::timeout, this, &AreaData::allowMessage);
}

static akashi::Area::Status toCoreStatus(AreaData::Status f_status);
static AreaData::Status fromCoreStatus(akashi::Area::Status f_status);

akashi::Area *AreaData::area() const
{
    return m_area;
}

const QMap<QString, AreaData::Status> AreaData::map_statuses = {
    {"idle", AreaData::Status::IDLE},
    {"rp", AreaData::Status::RP},
    {"casing", AreaData::Status::CASING},
    {"lfp", AreaData::Status::LOOKING_FOR_PLAYERS},
    {"looking-for-players", AreaData::Status::LOOKING_FOR_PLAYERS},
    {"recess", AreaData::Status::RECESS},
    {"gaming", AreaData::Status::GAMING},
};

// The old API and the world model order their lock values differently.
static AreaData::LockStatus fromCoreLock(akashi::Area::LockState f_state)
{
    switch (f_state) {
    case akashi::Area::LockState::Locked:
        return AreaData::LockStatus::LOCKED;
    case akashi::Area::LockState::Spectatable:
        return AreaData::LockStatus::SPECTATABLE;
    default:
        return AreaData::LockStatus::FREE;
    }
}

void AreaData::removeClient(int f_charId, int f_userId)
{
    if (f_charId != -1) {
        m_area->releaseCharacter(f_charId);
    }
    m_area->removePlayer(f_userId);
}

void AreaData::addClient(int f_charId, int f_userId)
{
    if (f_charId != -1) {
        m_area->takeCharacter(f_charId);
    }
    m_area->addPlayer(f_userId);
    Q_EMIT userJoinedArea(m_area->id(), f_userId);
    // Send out ambience as well. Use channel 1 for that
    Q_EMIT sendAreaPacketClient(akashi::Packet("MC", {m_currentAmbience, QString::number(-1), ConfigManager::serverNickname(), QString::number(1), QString::number(1)}), f_userId);
    // The name will never be shown as we are using a spectator ID. Still nice for people who network sniff.
    // We auto-loop this so you'll never sit in silence unless wanted.
    Q_EMIT sendAreaPacketClient(akashi::Packet("MC", {m_currentMusic, QString::number(-1), ConfigManager::serverNickname(), QString::number(1)}), f_userId);
}

QList<int> AreaData::owners() const
{
    return m_area->owners();
}

void AreaData::addOwner(int f_clientId)
{
    m_area->addOwner(f_clientId);
    m_area->invite(f_clientId);
}

bool AreaData::removeOwner(int f_clientId)
{
    m_area->removeOwner(f_clientId);
    m_area->uninvite(f_clientId);

    if (!m_area->hasOwners() && m_area->lockState() != akashi::Area::LockState::Free) {
        m_area->setLockState(akashi::Area::LockState::Free);
        return true;
    }

    return false;
}

bool AreaData::isBlankpostingAllowed() const
{
    return m_blankpostingAllowed;
}

void AreaData::toggleBlankposting()
{
    m_blankpostingAllowed = !m_blankpostingAllowed;
}

bool AreaData::isProtected() const
{
    return m_isProtected;
}

AreaData::LockStatus AreaData::lockStatus() const
{
    return fromCoreLock(m_area->lockState());
}

bool AreaData::isJukeboxEnabled() const
{
    return m_jukebox;
}

int AreaData::jukeboxQueueSize() const
{
    return m_jukebox_queue.size();
}

bool AreaData::isPlayEnabled() const
{
    return m_playcmd;
}

void AreaData::lock()
{
    m_area->setLockState(akashi::Area::LockState::Locked);
}

void AreaData::unlock()
{
    m_area->setLockState(akashi::Area::LockState::Free);
}

void AreaData::spectatable()
{
    m_area->setLockState(akashi::Area::LockState::Spectatable);
}

bool AreaData::invite(int f_clientId)
{
    return m_area->invite(f_clientId);
}

bool AreaData::uninvite(int f_clientId)
{
    return m_area->uninvite(f_clientId);
}

int AreaData::playerCount() const
{
    return m_area->playerCount();
}

QList<QTimer *> AreaData::timers() const
{
    return m_timers;
}

QString AreaData::name() const
{
    return m_area->name();
}

int AreaData::index() const
{
    return m_area->id();
}

QString AreaData::displayName() const
{
    return m_display_name;
}

QList<int> AreaData::charactersTaken() const
{
    return m_area->charactersTaken();
}

bool AreaData::changeCharacter(int f_from, int f_to)
{
    if (m_area->charactersTaken().contains(f_to)) {
        return false;
    }

    if (f_to != -1) {
        if (f_from != -1) {
            m_area->releaseCharacter(f_from);
        }
        m_area->takeCharacter(f_to);
        return true;
    }

    if (f_from != -1) {
        m_area->releaseCharacter(f_from);
    }

    return false;
}

QList<AreaData::Evidence> AreaData::evidence() const
{
    return m_evidence;
}

void AreaData::swapEvidence(int f_eviId1, int f_eviId2)
{
    m_evidence.swapItemsAt(f_eviId1, f_eviId2);
}

void AreaData::appendEvidence(const AreaData::Evidence &f_evi_r)
{
    m_evidence.append(f_evi_r);
}

void AreaData::deleteEvidence(int f_eviId)
{
    m_evidence.removeAt(f_eviId);
}

void AreaData::replaceEvidence(int f_eviId, const AreaData::Evidence &f_newEvi_r)
{
    m_evidence.replace(f_eviId, f_newEvi_r);
}

void AreaData::setEvidenceOwnerToAll(int f_eviId)
{
    if (f_eviId < 0 || f_eviId >= m_evidence.size()) {
        return;
    }

    Evidence &evidence = m_evidence[f_eviId];
    QString description = evidence.description;

    // Search for owner tag in description
    static const QRegularExpression ownerRegex("<owner=(.*?)>");
    QRegularExpressionMatch match = ownerRegex.match(description);

    if (match.hasMatch()) {
        // Replace existing owner tag with <owner=all>
        description.replace(ownerRegex, "<owner=all>");
    }
    else {
        // If no owner tag exists, add <owner=all> at the beginning
        description = "<owner=all>\n" + description;
    }

    evidence.description = description;
}

AreaData::Status AreaData::status() const
{
    return fromCoreStatus(m_area->status());
}

// The old API's status values and the world model's translate one-to-one.
static akashi::Area::Status toCoreStatus(AreaData::Status f_status)
{
    switch (f_status) {
    case AreaData::RP:
        return akashi::Area::Status::Rp;
    case AreaData::CASING:
        return akashi::Area::Status::Casing;
    case AreaData::LOOKING_FOR_PLAYERS:
        return akashi::Area::Status::LookingForPlayers;
    case AreaData::RECESS:
        return akashi::Area::Status::Recess;
    case AreaData::GAMING:
        return akashi::Area::Status::Gaming;
    default:
        return akashi::Area::Status::Idle;
    }
}

static AreaData::Status fromCoreStatus(akashi::Area::Status f_status)
{
    switch (f_status) {
    case akashi::Area::Status::Rp:
        return AreaData::RP;
    case akashi::Area::Status::Casing:
        return AreaData::CASING;
    case akashi::Area::Status::LookingForPlayers:
        return AreaData::LOOKING_FOR_PLAYERS;
    case akashi::Area::Status::Recess:
        return AreaData::RECESS;
    case akashi::Area::Status::Gaming:
        return AreaData::GAMING;
    default:
        return AreaData::IDLE;
    }
}

bool AreaData::changeStatus(const QString &f_newStatus_r)
{
    if (AreaData::map_statuses.contains(f_newStatus_r)) {
        m_area->setStatus(toCoreStatus(AreaData::map_statuses[f_newStatus_r]));
        return true;
    }

    return false;
}

QList<int> AreaData::invited() const
{
    return m_area->invited();
}

bool AreaData::isMusicAllowed() const
{
    return m_toggleMusic;
}

bool AreaData::isMessageAllowed() const
{
    return m_can_send_ic_messages;
}

bool AreaData::isWtceAllowed() const
{
    return m_can_send_wtce;
}

bool AreaData::isShoutAllowed() const
{
    return m_can_use_shouts;
}

bool AreaData::isMedievalMode() const
{
    return m_medieval_mode;
}

void AreaData::startMessageFloodguard(int f_duration)
{
    m_can_send_ic_messages = false;
    m_message_floodguard_timer->setSingleShot(true);
    m_message_floodguard_timer->start(f_duration);
}

void AreaData::toggleMusic()
{
    m_toggleMusic = !m_toggleMusic;
}

void AreaData::setEviMod(const EvidenceMod &f_eviMod_r)
{
    m_eviMod = f_eviMod_r;
}

void AreaData::setTestimonyRecording(const TestimonyRecording &f_testimonyRecording_r)
{
    m_testimonyRecording = f_testimonyRecording_r;
}

void AreaData::restartTestimony()
{
    m_testimonyRecording = TestimonyRecording::PLAYBACK;
    m_statement = 0;
}

void AreaData::clearTestimony()
{
    m_testimonyRecording = AreaData::TestimonyRecording::STOPPED;
    m_statement = -1;
    m_testimony.clear();
}

bool AreaData::forceImmediate() const
{
    return m_forceImmediate;
}

void AreaData::toggleImmediate()
{
    m_forceImmediate = !m_forceImmediate;
}

const QStringList &AreaData::lastICMessage() const
{
    return m_lastICMessage;
}

void AreaData::updateLastICMessage(const QStringList &f_lastMessage_r)
{
    m_lastICMessage = f_lastMessage_r;
}

QStringList AreaData::judgelog() const
{
    return m_judgelog;
}

void AreaData::appendJudgelog(const QString &f_newLog_r)
{
    if (m_judgelog.size() == 10) {
        m_judgelog.removeFirst();
    }

    m_judgelog.append(f_newLog_r);
}

int AreaData::statement() const
{
    return m_statement;
}

void AreaData::recordStatement(const QStringList &f_newStatement_r)
{
    ++m_statement;
    m_testimony.append(f_newStatement_r);
}

void AreaData::addStatement(int f_position, const QStringList &f_newStatement_r)
{
    m_testimony.insert(f_position, f_newStatement_r);
}

void AreaData::replaceStatement(int f_position, const QStringList &f_newStatement_r)
{
    m_testimony.replace(f_position, f_newStatement_r);
}

void AreaData::removeStatement(int f_position)
{
    m_testimony.remove(f_position);
    --m_statement;
}

QPair<QStringList, AreaData::TestimonyProgress> AreaData::jumpToStatement(int f_position)
{
    m_statement = f_position;

    if (m_statement > m_testimony.size() - 1) {
        m_statement = 1;
        return {m_testimony.at(m_statement), TestimonyProgress::LOOPED};
    }
    if (m_statement <= 1) {
        m_statement = 1;
        return {m_testimony.at(m_statement), TestimonyProgress::STAYED_AT_FIRST};
    }
    else {
        return {m_testimony.at(m_statement), TestimonyProgress::OK};
    }
}

const QVector<QStringList> &AreaData::testimony() const
{
    return m_testimony;
}

AreaData::TestimonyRecording AreaData::testimonyRecording() const
{
    return m_testimonyRecording;
}

AreaData::EvidenceMod AreaData::eviMod() const
{
    return m_eviMod;
}

bool AreaData::addNotecard(const QString &f_owner_r, const QString &f_notecard_r)
{
    m_notecards[f_owner_r] = f_notecard_r;

    if (f_notecard_r.isNull()) {
        m_notecards.remove(f_owner_r);
        return false;
    }

    return true;
}

QStringList AreaData::notecards()
{
    QMapIterator<QString, QString> l_noteIter(m_notecards);
    QStringList l_notecards;

    while (l_noteIter.hasNext()) {
        l_noteIter.next();
        l_notecards << l_noteIter.key() << ": " << l_noteIter.value() << "\n";
    }

    m_notecards.clear();

    return l_notecards;
}

QString AreaData::musicPlayerBy() const
{
    return m_musicPlayedBy;
}

void AreaData::setMusicPlayedBy(const QString &f_music_player)
{
    m_musicPlayedBy = f_music_player;
}

void AreaData::changeMusic(const QString &f_source_r, const QString &f_newSong_r)
{
    m_currentMusic = f_newSong_r;
    m_musicPlayedBy = f_source_r;
}

void AreaData::changeAmbience(const QString &f_newSong_r)
{
    m_currentAmbience = f_newSong_r;
}

QString AreaData::currentMusic() const
{
    return m_currentMusic;
}

QString AreaData::currentAmbience() const
{
    return m_currentAmbience;
}

void AreaData::setCurrentMusic(QString f_current_song)
{
    m_currentMusic = f_current_song;
}

int AreaData::proHP() const
{
    return m_proHP;
}

void AreaData::changeHP(AreaData::Side f_side, int f_newHP)
{
    if (f_side == Side::DEFENCE) {
        m_defHP = std::min(std::max(0, f_newHP), 10);
    }
    else if (f_side == Side::PROSECUTOR) {
        m_proHP = std::min(std::max(0, f_newHP), 10);
    }
}

int AreaData::defHP() const
{
    return m_defHP;
}

QString AreaData::document() const
{
    return m_document;
}

void AreaData::changeDoc(const QString &f_newDoc_r)
{
    m_document = f_newDoc_r;
}

QString AreaData::areaMessage() const
{
    return m_area_message.isEmpty() ? "No area message set." : m_area_message;
}

bool AreaData::sendAreaMessageOnJoin() const
{
    return m_send_area_message;
}

void AreaData::changeAreaMessage(const QString &f_newMessage_r)
{
    m_area_message = f_newMessage_r;
}

void AreaData::clearAreaMessage()
{
    changeAreaMessage(QString{});
}

bool AreaData::isBgLocked() const
{
    return m_bgLocked;
}

void AreaData::toggleBgLock()
{
    m_bgLocked = !m_bgLocked;
}

bool AreaData::isIniswapAllowed() const
{
    return m_iniswapAllowed;
}

void AreaData::toggleIniswap()
{
    m_iniswapAllowed = !m_iniswapAllowed;
}

bool AreaData::isShownameAllowed() const
{
    return m_shownameAllowed;
}

QString AreaData::background() const
{
    return m_background;
}

void AreaData::setBackground(const QString f_background)
{
    m_background = f_background;
    QSettings *ambience_data = ConfigManager::ambience();
    QString new_ambience = ambience_data->value(f_background + "/ambience").toString();
    if (new_ambience != "") {
        changeAmbience(new_ambience);
    }
    else {
        changeAmbience(""); // DON'T use ~stop.mp3 it overrides some code we don't want overridden
    }
}

QString AreaData::side() const
{
    return m_side;
}

void AreaData::setSide(const QString f_side)
{
    m_side = f_side;
}

bool AreaData::ignoreBgList()
{
    return m_ignoreBgList;
}

void AreaData::toggleIgnoreBgList()
{
    m_ignoreBgList = !m_ignoreBgList;
}

void AreaData::toggleAreaMessageJoin()
{
    m_send_area_message = !m_send_area_message;
}

void AreaData::toggleJukebox()
{
    m_jukebox = !m_jukebox;
    if (!m_jukebox) {
        m_jukebox_queue.clear();
        m_jukebox_timer->stop();
    }
}

void AreaData::toggleWtceAllowed()
{
    m_can_send_wtce = !m_can_send_wtce;
}

void AreaData::toggleShoutAllowed()
{
    m_can_use_shouts = !m_can_use_shouts;
}

void AreaData::toggleMedievalMode()
{
    m_medieval_mode = !m_medieval_mode;
}

QString AreaData::addJukeboxSong(QString f_song)
{
    if (!m_jukebox_queue.contains(f_song)) {
        // Retrieve song information.
        QPair<QString, float> l_song = m_music_manager->songInformation(f_song, index());

        if (l_song.second > 0) {
            if (m_jukebox_queue.size() == 0) {

                Q_EMIT sendAreaPacket(akashi::Packet("MC", {l_song.first, QString::number(-1)}), index());
                m_jukebox_timer->start(l_song.second * 1000);
                setCurrentMusic(f_song);
                setMusicPlayedBy("Jukebox");
            }
            m_jukebox_queue.append(f_song);
            return "Song added to Jukebox.";
        }
        else {
            return "Unable to add song. Duration shorter than 1.";
        }
    }
    return "Unable to add song. Song already in Jukebox.";
}

QVector<int> AreaData::joinedIDs() const
{
    return m_area->players();
}

void AreaData::switchJukeboxSong()
{
    QString l_song_name;
    if (m_jukebox_queue.size() == 1) {
        l_song_name = m_jukebox_queue[0];
        QPair<QString, float> l_song = m_music_manager->songInformation(l_song_name, index());
        Q_EMIT sendAreaPacket(akashi::Packet("MC", {l_song.first, "-1"}), m_area->id());
        m_jukebox_timer->start(l_song.second * 1000);
    }
    else {
        int l_random_index = QRandomGenerator::system()->bounded(m_jukebox_queue.size());
        l_song_name = m_jukebox_queue[l_random_index];

        QPair<QString, float> l_song = m_music_manager->songInformation(l_song_name, index());
        Q_EMIT sendAreaPacket(akashi::Packet("MC", {l_song.first, "-1"}), m_area->id());
        m_jukebox_timer->start(l_song.second * 1000);

        m_jukebox_queue.remove(l_random_index);
        m_jukebox_queue.squeeze();
    }
    setCurrentMusic(l_song_name);
    setMusicPlayedBy("Jukebox");
}

void AreaData::allowMessage()
{
    m_can_send_ic_messages = true;
}

int AreaData::evidenceIndexByVisibleIndex(int f_visibleIndex, const QString &f_clientPos, bool f_isCM) const
{
    if (f_visibleIndex <= 0) {
        return -1;
    }

    int visibleCount = 0;
    for (int i = 0; i < m_evidence.size(); ++i) {
        const Evidence &evidence = m_evidence[i];

        // Apply the same filtering logic as in updateEvidenceList
        if (!f_isCM && m_eviMod == EvidenceMod::HIDDEN_CM) {
            static const QRegularExpression ownerRegex("<owner=(.*?)>");
            QRegularExpressionMatch match = ownerRegex.match(evidence.description);
            if (match.hasMatch()) {
                QStringList owners = match.captured(1).split(",");
                if (!owners.contains("all", Qt::CaseSensitivity::CaseInsensitive) &&
                    !owners.contains(f_clientPos, Qt::CaseSensitivity::CaseInsensitive)) {
                    continue; // This evidence is not visible to the client
                }
            }
            // no match = show it to all
        }

        // This evidence is visible, increment counter
        ++visibleCount;
        if (visibleCount == f_visibleIndex) {
            return i; // Return the real index
        }
    }

    return -1; // Not found
}

int AreaData::visibleIndexByEvidenceIndex(int f_evidenceIndex, const QString &f_clientPos, bool f_isCM) const
{
    if (f_evidenceIndex < 0 || f_evidenceIndex >= m_evidence.size()) {
        return 0; // Invalid index or not visible
    }

    int visibleCount = 0;
    for (int i = 0; i < m_evidence.size(); ++i) {
        const Evidence &evidence = m_evidence[i];

        // Apply the same filtering logic as in updateEvidenceList
        if (!f_isCM && m_eviMod == EvidenceMod::HIDDEN_CM) {
            static const QRegularExpression ownerRegex("<owner=(.*?)>");
            QRegularExpressionMatch match = ownerRegex.match(evidence.description);
            if (match.hasMatch()) {
                QStringList owners = match.captured(1).split(",");
                if (!owners.contains("all", Qt::CaseSensitivity::CaseInsensitive) &&
                    !owners.contains(f_clientPos, Qt::CaseSensitivity::CaseInsensitive)) {
                    continue; // This evidence is not visible to the client
                }
            }
            // no match = show it to all
        }

        // This evidence is visible, increment counter
        ++visibleCount;
        if (i == f_evidenceIndex) {
            return visibleCount; // Return the visible index (1-based)
        }
    }

    return 0; // Evidence not visible to this client
}
