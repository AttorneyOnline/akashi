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
#include "include/logger/u_logger.h"

ULogger::ULogger(QObject* parent) :
    QObject(parent)
{

}

void ULogger::logIC(const QString& f_charName, const QString& f_oocName, const QString& f_ipid, const QString& f_areaName)
{

}

void ULogger::logOOC(const QString& f_charName, const QString& f_oocName, const QString& f_ipid, const QString& f_areaName)
{

}

void ULogger::logLogin(const QString& f_charName, const QString& f_oocName, const QString& f_moderatorName,
                       const QString& f_ipid, bool& f_sucees)
{

}

void ULogger::logCMD(const QString& f_charName, const QString& f_oocName, const QString f_command, const QString f_Args)
{

}

void ULogger::logKick(const QString& f_moderator, const QString& f_targetIPID, const QString& f_targetName, const QString f_targetOOCName)
{

}

void ULogger::logBan(const QString &f_moderator, const QString &f_targetIPID, const QString &f_targetName, const QString f_targetOOCName, const QDateTime &duration)
{

}

void ULogger::logConnectionAttempt(const QString& ip_address, const QString& ipid, const QString& hdid)
{

}

void ULogger::updateAreaBuffer(const QString& f_areaName, const QString& f_entry)
{
    QQueue<QString>l_buffer = m_bufferMap.value(f_areaName);
    if (l_buffer.length() <= ConfigManager::logBuffer()) {
        l_buffer.enqueue(f_entry);
    }
    else {
        l_buffer.dequeue();
        l_buffer.enqueue(f_entry);
    }
    m_bufferMap.insert(f_areaName, l_buffer);
}

QQueue<QString> ULogger::buffer(const QString& f_areaName)
{
    return m_bufferMap.value(f_areaName);
}
