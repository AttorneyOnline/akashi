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
#ifndef LOGGER_H
#define LOGGER_H

#include "include/aoclient.h"
#include "include/aopacket.h"

#include <QFile>
#include <QDebug>
#include <QString>
#include <QQueue>
#include <QDateTime>

class AOClient;
class Logger
{
public:
    Logger(int p_max_length);

    void logIC(AOClient* client, AOPacket* packet);
    void flush();

private:
    void addEntry(QString entry);

    int max_length;
    QQueue<QString> buffer;
};

#endif // LOGGER_H
