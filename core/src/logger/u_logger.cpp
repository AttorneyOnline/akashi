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

    switch (ConfigManager::loggingType()) {
        case DataTypes::LogType::MODCALL :
            writerModcall = new WriterModcall;
            break;
        case DataTypes::LogType::FULL :
            writerFull = new WriterFull;
            break;
        case DataTypes::LogType::SQL :
            writerSQL = new WriterSQL;
            break;
    }
}

ULogger::~ULogger()
{
    switch (ConfigManager::loggingType()) {
        case DataTypes::LogType::MODCALL :
            writerModcall->deleteLater();
            break;
        case DataTypes::LogType::FULL :
            writerFull->deleteLater();
            break;
        case DataTypes::LogType::SQL :
            writerSQL->deleteLater();
            break;
    }
}

void ULogger::logIC(const QString& f_charName, const QString& f_oocName, const QString& f_ipid,
                    const QString& f_areaName, const QString& f_message)
{
    QString l_time = QDateTime::currentDateTime().toString("ddd MMMM d yyyy | hh:mm:ss");
    QString l_logEntry = QStringLiteral("[%1][%5][IC][%2(%3)][%4]%6")
            .arg(l_time, f_charName, f_oocName, f_ipid, f_areaName, f_message);
    updateAreaBuffer(f_areaName,l_logEntry);
}

void ULogger::logOOC(const QString& f_charName, const QString& f_oocName, const QString& f_ipid,
                     const QString& f_areaName, const QString& f_message)
{
    QString l_time = QDateTime::currentDateTime().toString("ddd MMMM d yyyy | hh:mm:ss");
    QString l_logEntry = QStringLiteral("[%1][%5][OOC][%2(%3)][%4]%6")
            .arg(l_time, f_charName, f_oocName, f_ipid, f_areaName, f_message);
    updateAreaBuffer(f_areaName,l_logEntry);
}

void ULogger::logLogin(const QString& f_charName, const QString& f_oocName, const QString& f_moderatorName,
                       const QString& f_ipid, const QString& f_areaName, const bool &f_success)
{
    QString l_time = QDateTime::currentDateTime().toString("ddd MMMM d yyyy | hh:mm:ss");
    QString l_success = f_success ? "[SUCCESS]" + f_moderatorName : "[FAILED]" + f_moderatorName;
    QString l_logEntry = QStringLiteral("[%1][LOGON][%2][%3][%4(%5)]")
            .arg(l_time, l_success, f_ipid, f_charName, f_oocName);
    updateAreaBuffer(f_areaName, l_logEntry);
}

void ULogger::logCMD(const QString& f_charName,const QString& f_ipid, const QString& f_oocName, const QString f_command,
                     const QStringList f_args, const QString f_areaName)
{
    QString l_time = QDateTime::currentDateTime().toString("ddd MMMM d yyyy | hh:mm:ss");
    QString l_logEntry;
    // Some commands contain sensitive data, like passwords
    // These must be filtered out
    if (f_command == "login") {
        l_logEntry = QStringLiteral("[%1][%2][LOGIN][%5][%3(%4)]")
                .arg(l_time, f_areaName, f_charName, f_oocName, f_ipid);
    }
    else if (f_command == "rootpass") {
        l_logEntry = QStringLiteral("[%1][%2][ROOTPASS][%5][%3(%4)]")
                .arg(l_time, f_areaName, f_charName, f_oocName, f_ipid);
    }
    else if (f_command == "adduser" && !f_args.isEmpty()) {
        l_logEntry = QStringLiteral("[%1][%2][USERADD][%6][%3(%4)]%5")
                .arg(l_time, f_areaName, f_charName, f_oocName, f_args.at(0), f_ipid);
    }
    else {
        l_logEntry = QStringLiteral("[%1][%2][CMD][%7][%3(%4)][%5]%6")
                .arg(l_time, f_areaName, f_charName, f_oocName, f_command, f_args.join(" "), f_ipid);
    }
    updateAreaBuffer(f_areaName,l_logEntry);
}

void ULogger::logKick(const QString& f_moderator, const QString& f_targetIPID, const QString& f_targetName, const QString f_targetOOCName)
{
    QString l_time = QDateTime::currentDateTime().toString("ddd MMMM d yyyy | hh:mm:ss");
    QString l_logEntry = QStringLiteral("[%1][KICK][%2][%3][%4(%5)]")
            .arg(l_time, f_moderator, f_targetIPID, f_targetName, f_targetOOCName);
    updateAreaBuffer("SERVER",l_logEntry);
}

void ULogger::logBan(const QString &f_moderator, const QString &f_targetIPID, const QString &f_targetName, const QString f_targetOOCName, const QString &f_duration)
{
    QString l_time = QDateTime::currentDateTime().toString("ddd MMMM d yyyy | hh:mm:ss");
    QString l_logEntry = QStringLiteral("[%1][BAN][%2][%3(%4)][%5][%6]")
            .arg(l_time, f_moderator, f_targetName, f_targetOOCName, f_targetIPID, f_duration);
    updateAreaBuffer("SERVER",l_logEntry);
}

void ULogger::logModcall(const QString &f_charName, const QString &f_ipid, const QString &f_oocName, const QString &f_areaName)
{
    QString l_time = QDateTime::currentDateTime().toString("ddd MMMM d yyyy | hh:mm:ss");
    QString l_logEvent = QStringLiteral("[%1][%2][MODCALL][%5][%3(%4)]")
            .arg(l_time, f_areaName, f_charName, f_oocName, f_ipid);
    updateAreaBuffer(f_areaName, l_logEvent);

    if (ConfigManager::loggingType() == DataTypes::LogType::MODCALL) {
        writerModcall->flush(f_areaName, buffer(f_areaName));
    }
}

void ULogger::logConnectionAttempt(const QString& f_ip_address, const QString& f_ipid, const QString& f_hdid)
{
    QString l_time = QDateTime::currentDateTime().toString("ddd MMMM d yyyy | hh:mm:ss");
    QString l_logEntry = QStringLiteral("[%1][CONNECT][%2][%3][%4]\n")
            .arg(l_time, f_ip_address, f_ipid, f_hdid);
    updateAreaBuffer("SERVER",l_logEntry);
}

void ULogger::updateAreaBuffer(const QString& f_areaName, const QString& f_logEntry)
{
    QQueue<QString>l_buffer = m_bufferMap.value(f_areaName);

    if (l_buffer.length() <= ConfigManager::logBuffer()) {
        l_buffer.enqueue(f_logEntry);
    }
    else {
        l_buffer.dequeue();
        l_buffer.enqueue(f_logEntry);
    }
    m_bufferMap.insert(f_areaName, l_buffer);
}

QQueue<QString> ULogger::buffer(const QString& f_areaName)
{
    return m_bufferMap.value(f_areaName);
}
