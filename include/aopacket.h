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
#ifndef PACKET_MANAGER_H
#define PACKET_MANAGER_H

#include <QByteArray>
#include <QDebug>
#include <QString>
#include <QStringList>

class AOPacket {
  public:
    AOPacket(QString p_header, QStringList p_contents);
    AOPacket(QString packet);
    QString toString();
    QByteArray toUtf8();

    QString header;
    QStringList contents;
};

#endif // PACKET_MANAGER_H
