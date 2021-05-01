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
    QString configured_evi_mod = areas_ini.value("evidence_mod", "FFA").toString().toLower();
    m_blankpostingAllowed = areas_ini.value("blankposting_allowed","true").toBool();
    m_forceImmediate = areas_ini.value("force_immediate", "false").toBool();
    m_toggleMusic = areas_ini.value("toggle_music", "true").toBool();
    m_shownameAllowed = areas_ini.value("shownames_allowed", "true").toBool();
    areas_ini.endGroup();
    QSettings config_ini("config/config.ini", QSettings::IniFormat);
    config_ini.setIniCodec("UTF-8");
    config_ini.beginGroup("Options");
    int log_size = config_ini.value("logbuffer", 50).toInt();
    QString l_logType = config_ini.value("logger","modcall").toString();
    config_ini.endGroup();
    if (log_size == 0)
        log_size = 500;
    m_logger = new Logger(log_size, l_logType);
    QTimer* timer1 = new QTimer();
    m_timers.append(timer1);
    QTimer* timer2 = new QTimer();
    m_timers.append(timer2);
    QTimer* timer3 = new QTimer();
    m_timers.append(timer3);
    QTimer* timer4 = new QTimer();
    m_timers.append(timer4);

    if (configured_evi_mod == "cm")
        m_eviMod = EvidenceMod::CM;
    else if (configured_evi_mod == "mod")
        m_eviMod = EvidenceMod::MOD;
    else if (configured_evi_mod == "hiddencm")
        m_eviMod = EvidenceMod::HIDDEN_CM;
    else
        m_eviMod = EvidenceMod::FFA;
}

void AreaData::clientLeftArea(int f_charId)
{
    --m_playerCount;
    m_charactersTaken.removeAll(f_charId);
}

QList<int> AreaData::owners() const
{
    QString l_test;
    const auto& l_buffer = m_logger->m_buffer;
    for (const auto& l_item : l_buffer)
    {
        l_test.append(l_item + "\n");
    }

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

bool AreaData::invite(int f_clientId)
{
    if (m_invited.contains(f_clientId)) {
        return false;
    }

    m_invited.append(f_clientId);
    return true;
}

int AreaData::playerCount() const
{
    return m_playerCount;
}

void AreaData::changePlayerCount(bool f_increase)
{
    f_increase ? m_playerCount++: m_playerCount--;
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

QList<AreaData::Evidence> AreaData::evidence() const
{
    return m_evidence;
}

AreaData::Status AreaData::status() const
{
    return m_status;
}

QList<int> AreaData::invited() const
{
    return m_invited;
}

AreaData::LockStatus AreaData::locked() const
{
    return m_locked;
}

bool AreaData::isMusicAllowed() const
{
    return m_toggleMusic;
}

void AreaData::toggleMusic()
{
    m_toggleMusic = !m_toggleMusic;
}

void AreaData::setTestimonyRecording(const TestimonyRecording &testimonyRecording)
{
    m_testimonyRecording = testimonyRecording;
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

QStringList AreaData::lastICMessage() const
{
    return m_lastICMessage;
}

void AreaData::updateLastICMessage(const QStringList &f_lastMessage)
{
    m_lastICMessage = f_lastMessage;
}

QStringList AreaData::judgelog() const
{
    return m_judgelog;
}

int AreaData::statement() const
{
    return m_statement;
}

void AreaData::recordStatement(const QStringList &f_newStatement)
{
    ++m_statement;
    m_testimony.append(f_newStatement);
}

void AreaData::addStatement(int f_position, const QStringList &f_newStatement)
{
    m_testimony.insert(f_position, f_newStatement);
}

std::pair<QStringList, AreaData::TestimonyProgress> AreaData::advanceTestimony(bool f_forward)
{
    f_forward ? m_statement++: m_statement--;

    if (m_statement > m_testimony.size() - 1) {
        return {m_testimony.at(m_statement), TestimonyProgress::LOOPED};
    }
    if (m_statement <= 0) {
        return {m_testimony.at(m_statement), TestimonyProgress::STAYED_AT_FIRST};
    }
    else {
        return {m_testimony.at(m_statement), TestimonyProgress::OK};
    }
}

QStringList AreaData::jumpToStatement(int f_statementNr)
{
    m_statement = f_statementNr;
    return m_testimony.at(m_statement);
}

QVector<QStringList> AreaData::testimony() const
{
    return m_testimony;
}

AreaData::TestimonyRecording AreaData::testimonyRecording() const
{
    return m_testimonyRecording;
}

QMap<QString, QString> AreaData::notecards() const
{
    return m_notecards;
}

AreaData::EvidenceMod AreaData::eviMod() const
{
    return m_eviMod;
}

QString AreaData::musicPlayerBy() const
{
    return m_musicPlayerBy;
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

bool AreaData::bgLocked() const
{
    return m_bgLocked;
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
