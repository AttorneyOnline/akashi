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

#include <algorithm>

#include "include/area_data.h"

AreaData::AreaData(QString p_name, int p_index) :
    m_index(p_index),
    m_playerCount(0),
    m_status(IDLE),
    m_locked(FREE),
    m_document("No document."),
    m_defHP(10),
    m_proHP(10),
    m_statement(0),
    m_judgelog(),
    m_lastICMessage()
{
    QStringList name_split = p_name.split(":");
    name_split.removeFirst();
    m_name = name_split.join(":");
    QSettings areas_ini("config/areas.ini", QSettings::IniFormat);
    areas_ini.setIniCodec("UTF-8");
    areas_ini.beginGroup(p_name);
    m_background = areas_ini.value("background", "gs4").toString();
    m_isProtected = areas_ini.value("protected_area", "false").toBool();
    m_iniswapAllowed = areas_ini.value("iniswap_allowed", "true").toBool();
    m_bgLocked = areas_ini.value("bg_locked", "false").toBool();
    m_eviMod = QVariant(areas_ini.value("evidence_mod", "FFA").toString().toUpper()).value<EvidenceMod>();
    m_blankpostingAllowed = areas_ini.value("blankposting_allowed","true").toBool();
    m_forceImmediate = areas_ini.value("force_immediate", "false").toBool();
    m_toggleMusic = areas_ini.value("toggle_music", "true").toBool();
    m_shownameAllowed = areas_ini.value("shownames_allowed", "true").toBool();
    m_ignoreBgList = areas_ini.value("ignore_bglist", "false").toBool();
    areas_ini.endGroup();
    int log_size = ConfigManager::logBuffer();
    DataTypes::LogType l_logType = ConfigManager::loggingType();
    if (log_size == 0)
        log_size = 500;
    m_logger = new Logger(m_name, log_size, l_logType);
    QTimer* timer1 = new QTimer();
    m_timers.append(timer1);
    QTimer* timer2 = new QTimer();
    m_timers.append(timer2);
    QTimer* timer3 = new QTimer();
    m_timers.append(timer3);
    QTimer* timer4 = new QTimer();
    m_timers.append(timer4);
}

const QMap<QString, AreaData::Status> AreaData::map_statuses = {
    {"idle",                    AreaData::Status::IDLE                },
    {"rp",                      AreaData::Status::RP                  },
    {"casing",                  AreaData::Status::CASING              },
    {"lfp",                     AreaData::Status::LOOKING_FOR_PLAYERS },
    {"looking-for-players",     AreaData::Status::LOOKING_FOR_PLAYERS },
    {"recess",                  AreaData::Status::RECESS              },
    {"gaming",                  AreaData::Status::GAMING              },
};

void AreaData::clientLeftArea(int f_charId)
{
    --m_playerCount;

    if (f_charId != -1) {
        m_charactersTaken.removeAll(f_charId);
    }
}

void AreaData::clientJoinedArea(int f_charId)
{
    ++m_playerCount;

    if (f_charId != -1) {
        m_charactersTaken.append(f_charId);
    }
}

QList<int> AreaData::owners() const
{
    return m_owners;
}

void AreaData::addOwner(int f_clientId)
{
    m_owners.append(f_clientId);
    m_invited.append(f_clientId);
}

bool AreaData::removeOwner(int f_clientId)
{
    m_owners.removeAll(f_clientId);
    m_invited.removeAll(f_clientId);

    if (m_owners.isEmpty() && m_locked != AreaData::FREE) {
        m_locked = AreaData::FREE;
        return true;
    }

    return false;
}

bool AreaData::blankpostingAllowed() const
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
    return m_locked;
}

void AreaData::lock()
{
    m_locked = LockStatus::LOCKED;
}

void AreaData::unlock()
{
    m_locked = LockStatus::FREE;
}

void AreaData::spectatable()
{
    m_locked = LockStatus::SPECTATABLE;
}

bool AreaData::invite(int f_clientId)
{
    if (m_invited.contains(f_clientId)) {
        return false;
    }

    m_invited.append(f_clientId);
    return true;
}

bool AreaData::uninvite(int f_clientId)
{
    if (!m_invited.contains(f_clientId)) {
        return false;
    }

    m_invited.removeAll(f_clientId);
    return true;
}

int AreaData::playerCount() const
{
    return m_playerCount;
}

QList<QTimer *> AreaData::timers() const
{
    return m_timers;
}

QString AreaData::name() const
{
    return m_name;
}

int AreaData::index() const
{
    return m_index;
}

QList<int> AreaData::charactersTaken() const
{
    return m_charactersTaken;
}

bool AreaData::changeCharacter(int f_from, int f_to)
{
    if (m_charactersTaken.contains(f_to)) {
        return false;
    }

    if (f_to != -1) {
        if (f_from != -1) {
            m_charactersTaken.removeAll(f_from);
        }
        m_charactersTaken.append(f_to);
        return true;
    }

    if (f_to == -1 && f_from != -1) {
        m_charactersTaken.removeAll(f_from);
    }

    return false;
}

QList<AreaData::Evidence> AreaData::evidence() const
{
    return m_evidence;
}

void AreaData::swapEvidence(int f_eviId1, int f_eviId2)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 13, 0)
    //swapItemsAt does not exist in Qt older than 5.13
    m_evidence.swap(f_eviId1, f_eviId2);
#else
    m_evidence.swapItemsAt(f_eviId1, f_eviId2);
#endif
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

AreaData::Status AreaData::status() const
{
    return m_status;
}

bool AreaData::changeStatus(const QString &f_newStatus_r)
{
    if (AreaData::map_statuses.contains(f_newStatus_r)) {
        m_status = AreaData::map_statuses[f_newStatus_r];
        return true;
    }

    return false;
}

QList<int> AreaData::invited() const
{
    return m_invited;
}

bool AreaData::isMusicAllowed() const
{
    return m_toggleMusic;
}

void AreaData::toggleMusic()
{
    m_toggleMusic = !m_toggleMusic;
}

void AreaData::log(const QString &f_clientName_r, const QString &f_clientIpid_r, const AOPacket &f_packet_r) const
{
    auto l_header = f_packet_r.header;

    if (l_header == "MS") {
        m_logger->logIC(f_clientName_r, f_clientIpid_r, f_packet_r.contents.at(4), f_packet_r.contents.at(15));
    } else if (l_header == "CT") {
        m_logger->logOOC(f_clientName_r, f_clientIpid_r, f_packet_r.contents.at(1));
    } else if (l_header == "ZZ") {
        m_logger->logModcall(f_clientName_r, f_clientIpid_r, f_packet_r.contents.at(0));
    }
}

void AreaData::logLogin(const QString &f_clientName_r, const QString &f_clientIpid_r, bool f_success, const QString& f_modname_r) const
{
    m_logger->logLogin(f_clientName_r, f_clientIpid_r, f_success, f_modname_r);
}

void AreaData::logCmd(const QString &f_clientName_r, const QString &f_clientIpid_r, const QString &f_command_r, const QStringList &f_cmdArgs_r) const
{
    m_logger->logCmd(f_clientName_r, f_clientIpid_r, f_command_r, f_cmdArgs_r);
}

void AreaData::flushLogs() const
{
    m_logger->flush();
}

void AreaData::setEviMod(const EvidenceMod &f_eviMod_r)
{
    m_eviMod = f_eviMod_r;
}

QQueue<QString> AreaData::buffer() const
{
    return m_logger->buffer();
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

const QStringList& AreaData::lastICMessage() const
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

const QVector<QStringList>& AreaData::testimony() const
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

QStringList AreaData::getNotecards()
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

void AreaData::changeMusic(const QString &f_source_r, const QString &f_newSong_r)
{
    m_currentMusic = f_newSong_r;
    m_musicPlayedBy = f_source_r;
}

QString AreaData::currentMusic() const
{
    return m_currentMusic;
}

int AreaData::proHP() const
{
    return m_proHP;
}

void AreaData::changeHP(AreaData::Side f_side, int f_newHP)
{
    if (f_side == Side::DEFENCE) {
        m_defHP = std::min(std::max(0, f_newHP), 10);
    } else if(f_side == Side::PROSECUTOR) {
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

bool AreaData::bgLocked() const
{
    return m_bgLocked;
}

void AreaData::toggleBgLock()
{
    m_bgLocked = !m_bgLocked;
}

bool AreaData::iniswapAllowed() const
{
    return m_iniswapAllowed;
}

void AreaData::toggleIniswap()
{
    m_iniswapAllowed = !m_iniswapAllowed;
}

bool AreaData::shownameAllowed() const
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
}

bool AreaData::ignoreBgList()
{
    return m_ignoreBgList;
}

void AreaData::toggleIgnoreBgList()
{
    m_ignoreBgList = !m_ignoreBgList;
}
