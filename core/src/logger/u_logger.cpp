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

void ULogger::logIC(MessageLog f_log)
{
    Q_UNUSED(f_log)
}

void ULogger::logOOC(MessageLog f_log)
{
    Q_UNUSED(f_log)
}

void ULogger::logLogin(LoginLog f_log)
{
    Q_UNUSED(f_log)
}

void ULogger::logCMD(CommandLog f_log)
{
    Q_UNUSED(f_log)
}

void ULogger::logKick(ModerativeLog f_log)
{
    Q_UNUSED(f_log)
}

void ULogger::logBan(ModerativeLog f_log)
{
    Q_UNUSED(f_log)
}

void ULogger::logConnectionAttempt(ConnectionLog f_log)
{
    Q_UNUSED(f_log)
}

void ULogger::updateAreaBuffer(const QString& f_areaName, const QString& f_entry)
{
    QQueue<QString>f_buffer = m_bufferMap.value(f_areaName);
    if (f_buffer.length() < ConfigManager::logBuffer()) {
        f_buffer.enqueue(f_entry);
    }
    else {
        f_buffer.dequeue();
        f_buffer.enqueue(f_entry);
    }
    m_bufferMap.insert(f_areaName, f_buffer);
}

QQueue<QString> ULogger::buffer(const QString& f_areaName)
{
    return m_bufferMap.value(f_areaName);
}
