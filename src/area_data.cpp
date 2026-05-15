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
#include "world/jukebox.h"

#include <QMetaEnum>
#include <QRegularExpression>

#include <algorithm>

static akashi::EvidenceStore::Access toStoreAccess(AreaData::EvidenceMod f_mod);
static AreaData::EvidenceMod fromStoreAccess(akashi::EvidenceStore::Access f_access);

AreaData::AreaData(QString p_name, int p_index, QSettings *f_areas_ini, QSettings *f_ambience_ini) :
    m_document("No document."),
    m_defHP(10),
    m_proHP(10),
    m_judgelog(),
    m_lastICMessage(),
    m_ambience_ini(f_ambience_ini)
{
    QStringList name_split = p_name.split(":");
    name_split.removeFirst();
    QString l_name = name_split.join(":");
    if (l_name.isEmpty()) {
        l_name = "Unnamed Area";
    }
    m_area = new akashi::Area(p_index, l_name, 0, p_index, this);
    m_display_name = "[" + QString::number(p_index) + "] " + l_name;
    QSettings *areas_ini = f_areas_ini;
    areas_ini->beginGroup(p_name);
    m_settings.background = areas_ini->value("background", "gs4").toString();
    m_settings.protected_area = areas_ini->value("protected_area", "false").toBool();
    m_settings.iniswap_allowed = areas_ini->value("iniswap_allowed", "true").toBool();
    m_settings.background_locked = areas_ini->value("bg_locked", "false").toBool();
    m_evidence_store.setAccess(toStoreAccess(QVariant(areas_ini->value("evidence_mod", "FFA").toString().toUpper()).value<EvidenceMod>()));
    m_settings.blankposting_allowed = areas_ini->value("blankposting_allowed", "true").toBool();
    m_settings.area_message = areas_ini->value("area_message").toString();
    m_settings.send_area_message_on_join = areas_ini->value("send_area_message_on_join", false).toBool();
    m_settings.force_immediate = areas_ini->value("force_immediate", "false").toBool();
    m_settings.music_allowed = areas_ini->value("toggle_music", "true").toBool();
    m_settings.shownames_allowed = areas_ini->value("shownames_allowed", "true").toBool();
    m_settings.ignore_background_list = areas_ini->value("ignore_bglist", "false").toBool();
    m_settings.jukebox_enabled = areas_ini->value("jukebox_enabled", "false").toBool();
    m_settings.play_command_enabled = areas_ini->value("playcmd_enabled", "false").toBool();
    m_settings.wtce_allowed = areas_ini->value("wtce_enabled", "true").toBool();
    m_settings.shouts_allowed = areas_ini->value("shouts_enabled", "true").toBool();
    areas_ini->endGroup();
    QTimer *timer1 = new QTimer();
    m_timers.append(timer1);
    QTimer *timer2 = new QTimer();
    m_timers.append(timer2);
    QTimer *timer3 = new QTimer();
    m_timers.append(timer3);
    QTimer *timer4 = new QTimer();
    m_timers.append(timer4);
    m_jukebox = new akashi::Jukebox(this);
    connect(m_jukebox, &akashi::Jukebox::songStarted, this, [this](const akashi::JukeboxSong &f_song) {
        m_jukebox->changeMusic(f_song.name, QStringLiteral("Jukebox"));
    });
    m_message_floodguard_timer = new QTimer(this);
    connect(m_message_floodguard_timer, &QTimer::timeout, this, &AreaData::allowMessage);
}

static QString toWireStatus(AreaData::Status f_status);
static AreaData::Status fromWireStatus(const QString &f_status);

// The old evidence mod values and the store's access values translate
// one-to-one.
static akashi::EvidenceStore::Access toStoreAccess(AreaData::EvidenceMod f_mod)
{
    switch (f_mod) {
    case AreaData::EvidenceMod::MOD:
        return akashi::EvidenceStore::Access::Mod;
    case AreaData::EvidenceMod::CM:
        return akashi::EvidenceStore::Access::Cm;
    case AreaData::EvidenceMod::HIDDEN_CM:
        return akashi::EvidenceStore::Access::HiddenCm;
    default:
        return akashi::EvidenceStore::Access::FreeForAll;
    }
}

static AreaData::EvidenceMod fromStoreAccess(akashi::EvidenceStore::Access f_access)
{
    switch (f_access) {
    case akashi::EvidenceStore::Access::Mod:
        return AreaData::EvidenceMod::MOD;
    case akashi::EvidenceStore::Access::Cm:
        return AreaData::EvidenceMod::CM;
    case akashi::EvidenceStore::Access::HiddenCm:
        return AreaData::EvidenceMod::HIDDEN_CM;
    default:
        return AreaData::EvidenceMod::FFA;
    }
}

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
    m_jukebox->playerLeft(f_userId);
    Q_EMIT userLeftArea(m_area->id(), f_userId);
}

void AreaData::addClient(int f_charId, int f_userId)
{
    if (f_charId != -1) {
        m_area->takeCharacter(f_charId);
    }
    m_area->addPlayer(f_userId);
    // The messenger layer reacts by catching the joiner up on the area's
    // music list, ambience and playing song - the world model itself
    // never touches the wire.
    Q_EMIT userJoinedArea(m_area->id(), f_userId);
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
    return m_settings.blankposting_allowed;
}

void AreaData::toggleBlankposting()
{
    m_settings.blankposting_allowed = !m_settings.blankposting_allowed;
}

bool AreaData::isProtected() const
{
    return m_settings.protected_area;
}

AreaData::LockStatus AreaData::lockStatus() const
{
    return fromCoreLock(m_area->lockState());
}

bool AreaData::isJukeboxEnabled() const
{
    return m_settings.jukebox_enabled;
}

bool AreaData::isPlayEnabled() const
{
    return m_settings.play_command_enabled;
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
    return m_evidence_store.items();
}

void AreaData::swapEvidence(int f_eviId1, int f_eviId2)
{
    m_evidence_store.swap(f_eviId1, f_eviId2);
}

void AreaData::appendEvidence(const AreaData::Evidence &f_evi_r)
{
    m_evidence_store.append(f_evi_r);
}

void AreaData::deleteEvidence(int f_eviId)
{
    m_evidence_store.remove(f_eviId);
}

void AreaData::replaceEvidence(int f_eviId, const AreaData::Evidence &f_newEvi_r)
{
    m_evidence_store.replace(f_eviId, f_newEvi_r);
}

void AreaData::setEvidenceOwnerToAll(int f_eviId)
{
    m_evidence_store.revealToAll(f_eviId);
}

AreaData::Status AreaData::status() const
{
    return fromWireStatus(m_area->status());
}

QString AreaData::statusLine() const
{
    return m_area->status();
}

// The world model stores the status as the line the area list shows; the
// well-known statuses of the old enum map onto their exact old spellings.
static QString toWireStatus(AreaData::Status f_status)
{
    return QVariant::fromValue(f_status).toString().replace("_", "-");
}

static AreaData::Status fromWireStatus(const QString &f_status)
{
    const QMetaEnum l_statuses = QMetaEnum::fromType<AreaData::Status>();
    bool l_known = false;
    const int l_value = l_statuses.keyToValue(QString(f_status).replace("-", "_").toUtf8(), &l_known);
    // A custom status line has no old enum value; it reads as IDLE there.
    return l_known ? static_cast<AreaData::Status>(l_value) : AreaData::IDLE;
}

bool AreaData::changeStatus(const QString &f_newStatus_r)
{
    if (AreaData::map_statuses.contains(f_newStatus_r)) {
        m_area->setStatus(toWireStatus(AreaData::map_statuses[f_newStatus_r]));
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
    return m_settings.music_allowed;
}

bool AreaData::isMessageAllowed() const
{
    return m_can_send_ic_messages;
}

bool AreaData::isWtceAllowed() const
{
    return m_settings.wtce_allowed;
}

bool AreaData::isShoutAllowed() const
{
    return m_settings.shouts_allowed;
}

bool AreaData::isMedievalMode() const
{
    return m_settings.medieval_mode;
}

void AreaData::startMessageFloodguard(int f_duration)
{
    m_can_send_ic_messages = false;
    m_message_floodguard_timer->setSingleShot(true);
    m_message_floodguard_timer->start(f_duration);
}

void AreaData::toggleMusic()
{
    m_settings.music_allowed = !m_settings.music_allowed;
}

void AreaData::setEviMod(const EvidenceMod &f_eviMod_r)
{
    m_evidence_store.setAccess(toStoreAccess(f_eviMod_r));
}

akashi::TestimonyRecorder *AreaData::testimonyRecorder()
{
    return &m_testimony_recorder;
}

bool AreaData::forceImmediate() const
{
    return m_settings.force_immediate;
}

void AreaData::toggleImmediate()
{
    m_settings.force_immediate = !m_settings.force_immediate;
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

AreaData::EvidenceMod AreaData::eviMod() const
{
    return fromStoreAccess(m_evidence_store.access());
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
    return m_jukebox->musicPlayedBy();
}

void AreaData::setMusicPlayedBy(const QString &f_music_player)
{
    m_jukebox->changeMusic(m_jukebox->currentSong(), f_music_player);
}

void AreaData::changeMusic(const QString &f_source_r, const QString &f_newSong_r)
{
    m_jukebox->changeMusic(f_newSong_r, f_source_r);
}

void AreaData::changeAmbience(const QString &f_newSong_r)
{
    m_jukebox->changeAmbience(f_newSong_r);
}

QString AreaData::currentMusic() const
{
    return m_jukebox->currentSong();
}

QString AreaData::currentAmbience() const
{
    return m_jukebox->currentAmbience();
}

void AreaData::setCurrentMusic(QString f_current_song)
{
    m_jukebox->changeMusic(f_current_song, m_jukebox->musicPlayedBy());
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
    return m_settings.area_message.isEmpty() ? "No area message set." : m_settings.area_message;
}

bool AreaData::sendAreaMessageOnJoin() const
{
    return m_settings.send_area_message_on_join;
}

void AreaData::changeAreaMessage(const QString &f_newMessage_r)
{
    m_settings.area_message = f_newMessage_r;
}

void AreaData::clearAreaMessage()
{
    changeAreaMessage(QString{});
}

bool AreaData::isBgLocked() const
{
    return m_settings.background_locked;
}

void AreaData::toggleBgLock()
{
    m_settings.background_locked = !m_settings.background_locked;
}

bool AreaData::isIniswapAllowed() const
{
    return m_settings.iniswap_allowed;
}

void AreaData::toggleIniswap()
{
    m_settings.iniswap_allowed = !m_settings.iniswap_allowed;
}

bool AreaData::isShownameAllowed() const
{
    return m_settings.shownames_allowed;
}

QString AreaData::background() const
{
    return m_settings.background;
}

void AreaData::setBackground(const QString f_background)
{
    m_settings.background = f_background;
    QString new_ambience = m_ambience_ini->value(f_background + "/ambience").toString();
    m_jukebox->changeAmbience(new_ambience.isEmpty() ? QString() : new_ambience);
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
    return m_settings.ignore_background_list;
}

void AreaData::toggleIgnoreBgList()
{
    m_settings.ignore_background_list = !m_settings.ignore_background_list;
}

void AreaData::toggleAreaMessageJoin()
{
    m_settings.send_area_message_on_join = !m_settings.send_area_message_on_join;
}

void AreaData::toggleJukebox()
{
    m_settings.jukebox_enabled = !m_settings.jukebox_enabled;
    if (m_settings.jukebox_enabled) {
        m_jukebox->start();
    }
    else {
        m_jukebox->stop();
    }
}

akashi::Jukebox *AreaData::jukebox() const
{
    return m_jukebox;
}

void AreaData::toggleWtceAllowed()
{
    m_settings.wtce_allowed = !m_settings.wtce_allowed;
}

void AreaData::toggleShoutAllowed()
{
    m_settings.shouts_allowed = !m_settings.shouts_allowed;
}

void AreaData::toggleMedievalMode()
{
    m_settings.medieval_mode = !m_settings.medieval_mode;
}

QVector<int> AreaData::joinedIDs() const
{
    return m_area->players();
}

void AreaData::allowMessage()
{
    m_can_send_ic_messages = true;
}

int AreaData::evidenceIndexByVisibleIndex(int f_visibleIndex, const QString &f_clientPos, bool f_isCM) const
{
    return m_evidence_store.itemIndexByVisibleIndex(f_visibleIndex, f_isCM, f_clientPos);
}

int AreaData::visibleIndexByEvidenceIndex(int f_evidenceIndex, const QString &f_clientPos, bool f_isCM) const
{
    return m_evidence_store.visibleIndexByItemIndex(f_evidenceIndex, f_isCM, f_clientPos);
}

QList<akashi::Evidence> AreaData::visibleEvidence(bool f_can_see_hidden, const QString &f_side) const
{
    return m_evidence_store.visibleItems(f_can_see_hidden, f_side);
}
