//////////////////////////////////////////////////////////////////////////////////////
//    akashi - a server for Attorney Online 2                                       //
//    Copyright (C) 2020  scatterflower                                           //
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
#include "include/logger.h"

Logger::Logger(int p_max_length)
{
    max_length = p_max_length;
}

void Logger::logIC(AOClient *client, AOPacket *packet)
{
    // TODO: copy pasted code
    QString time = QDateTime::currentDateTime().toString("ddd MMMM d yyyy | hh:mm:ss");
    QString area_name = client->getServer()->area_names.value(client->current_area);
    QString char_name = client->current_char;
    QString ipid = client->getIpid();
    QString message = packet->contents[4];

    QString log_entry = QStringLiteral("[%1][%2][IC] %3(%4): %5\n")
            .arg(time)
            .arg(area_name)
            .arg(char_name)
            .arg(ipid)
            .arg(message);
    addEntry(log_entry);
}

void Logger::logOOC(AOClient* client, AOPacket* packet)
{
    // TODO: copy pasted code
    QString time = QDateTime::currentDateTime().toString("ddd MMMM d yyyy | hh:mm:ss");
    QString area_name = client->getServer()->area_names.value(client->current_area);
    QString char_name = client->current_char;
    QString ipid = client->getIpid();
    QString message = packet->contents[1];

    QString log_entry = QStringLiteral("[%1][%2][OOC] %3(%4): %5\n")
            .arg(time)
            .arg(area_name)
            .arg(char_name)
            .arg(ipid)
            .arg(message);
    addEntry(log_entry);
}

void Logger::logModcall(AOClient* client, AOPacket* packet)
{
    // TODO: copy pasted code
    QString time = QDateTime::currentDateTime().toString("ddd MMMM d yyyy | hh:mm:ss");
    QString area_name = client->getServer()->area_names.value(client->current_area);
    QString char_name = client->current_char;
    QString ipid = client->getIpid();
    QString message = packet->contents[1];

    QString log_entry = QStringLiteral("[%1][%2][MODCALL] %3(%4): %5\n")
            .arg(time)
            .arg(area_name)
            .arg(char_name)
            .arg(ipid)
            .arg(message);
    addEntry(log_entry);
}

void Logger::addEntry(QString entry)
{
    if (buffer.length() < max_length) {
        buffer.enqueue(entry);
    }
    else {
        buffer.dequeue();
        buffer.enqueue(entry);
    }
}

void Logger::flush()
{
    QFile logfile("config/server.log");
    if (logfile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        QTextStream file_stream(&logfile);
        while (!buffer.isEmpty())
            file_stream << buffer.dequeue();
    }
    logfile.close();
}
