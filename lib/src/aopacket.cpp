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
#include "include/aopacket.h"

AOPacket::AOPacket(QString p_header, QStringList p_contents)
{
    header = p_header;
    contents = p_contents;
}

AOPacket::AOPacket(QString p_packet)
{
    QStringList packet_contents = p_packet.split("#");
    if (p_packet.at(0) == '#') {
        // The header is encrypted with FantaCrypt
        // This should never happen with AO2 2.4.3 or newer
        qDebug() << "FantaCrypt packet received";
        header = "Unknown";
        packet_contents.append("Unknown");
        return;
    }
    else {
        header = packet_contents[0];
    }
    packet_contents.removeFirst(); // Remove header
    packet_contents.removeLast();  // Remove anything trailing after delimiter
    contents = packet_contents;
}

QString AOPacket::toString()
{
    QString ao_packet = header;
    for (int i = 0; i < contents.length(); i++) {
        ao_packet += "#" + contents[i];
    }
    ao_packet += "#%";

    return ao_packet;
}

QByteArray AOPacket::toUtf8()
{
    QString packet_string = toString();
    return packet_string.toUtf8();
}
