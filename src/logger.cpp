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

Logger::Logger(int p_max_length, AreaData* p_area)
{
    area = p_area;
    max_length = p_max_length;
}

void Logger::logIC(AOClient *client, AOPacket *packet)
{
    QString message = packet->contents[4];
    addEntry(buildEntry(client, "IC", message));
}

void Logger::logOOC(AOClient* client, AOPacket* packet)
{
    QString message = packet->contents[1];
    addEntry(buildEntry(client, "OOC", message));
}

void Logger::logModcall(AOClient* client, AOPacket* packet)
{
    QString message = packet->contents[0];
    addEntry(buildEntry(client, "MODCALL", message));
}

void Logger::logCmd(AOClient *client, AOPacket *packet, QString cmd, QStringList args)
{
    // Some commands contain sensitive data, like passwords
    // These must be filtered out
    if (cmd == "login") {
        addEntry(buildEntry(client, "LOGIN", "Attempted login"));
    }
    else if (cmd == "rootpass") {
        addEntry(buildEntry(client, "USERS", "Root password created"));
    }
    else if (cmd == "adduser" && !args.isEmpty()) {
        addEntry(buildEntry(client, "USERS", "Added user " + args[0]));
    }
    else
        logOOC(client, packet);
}

void Logger::logLogin(AOClient *client, bool success, QString modname)
{
    QString message = success ? "Logged in as " + modname : "Failed to log in as " + modname;
    addEntry(buildEntry(client, "LOGIN", message));
}

QString Logger::buildEntry(AOClient *client, QString type, QString message)
{
    QString time = QDateTime::currentDateTime().toString("ddd MMMM d yyyy | hh:mm:ss");
    QString area_name = area->name;
    QString char_name = client->current_char;
    QString ipid = client->getIpid();

    QString log_entry = QStringLiteral("[%1][%2][%6] %3(%4): %5\n")
            .arg(time)
            .arg(area_name)
            .arg(char_name)
            .arg(ipid)
            .arg(message)
            .arg(type);
    return log_entry;
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
    // raiden suggested this, but idk if i want to use it
    // QString time = QDateTime::currentDateTime().toString("ddd mm/dd/yy hh:mm:ss");
    // QString filename = QStringLiteral("reports/%1/%2.log").arg(area->name).arg(time);
    QFile logfile("config/server.log");
    if (logfile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        QTextStream file_stream(&logfile);
        while (!buffer.isEmpty())
            file_stream << buffer.dequeue();
    }
    logfile.close();
}
