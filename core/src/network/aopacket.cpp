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
#include "include/network/aopacket.h"

AOPacket::AOPacket(QString p_header, QStringList p_contents) :
    m_header(p_header),
    m_content(p_contents),
    m_escaped(false)
{
}

AOPacket::AOPacket(QString f_packet)
{
    QString l_packet = f_packet;
    if (l_packet.isEmpty() || l_packet.at(0) == '#' || l_packet.contains("%")) {
#if NET_DEBUG
        qDebug() << "Invalid or fantacrypt packet received.";
#endif
        m_header = "Unknown";
        m_content = QStringList{"Unknown"};
        return;
    }

    QStringList l_split_packet = l_packet.split("#");
    m_header = l_split_packet.value(0);

    // Remove header and trailing packetFinished
    l_split_packet.removeFirst();
    l_split_packet.removeLast();
    m_content = l_split_packet;

    // All incoming data has to be escaped after being split.
    this->unescapeContent();
}

const QStringList AOPacket::getContent()
{
    return m_content;
}

QString AOPacket::getHeader()
{
    return m_header;
}

QString AOPacket::toString()
{
    if (!isPacketEscaped() && !(m_header == "LE")) {
        // We will never send unescaped data to a client, unless its evidence.
        this->escapeContent();
    }
    else {
        // Of course AO has SOME expection to the rule.
        this->escapeEvidence();
    }
    return QString("%1#%2#%3").arg(m_header, m_content.join("#"), packetFinished);
}

QByteArray AOPacket::toUtf8()
{
    QString l_packet = this->toString();
    return l_packet.toUtf8();
}

void AOPacket::setContentField(int f_content_index, QString f_content_data)
{
    m_content[f_content_index] = f_content_data;
}

void AOPacket::escapeContent()
{
    m_content.replaceInStrings("#", "<num>")
        .replaceInStrings("%", "<percent>")
        .replaceInStrings("$", "<dollar>")
        .replaceInStrings("&", "<and>");
    this->setPacketEscaped(true);
}

void AOPacket::unescapeContent()
{
    m_content.replaceInStrings("<num>", "#")
        .replaceInStrings("<percent>", "%")
        .replaceInStrings("<dollar>", "$")
        .replaceInStrings("<and>", "&");
    this->setPacketEscaped(false);
}

void AOPacket::escapeEvidence()
{
    m_content.replaceInStrings("#", "<num>")
        .replaceInStrings("%", "<percent>")
        .replaceInStrings("$", "<dollar>");
    this->setPacketEscaped(true);
}

void AOPacket::setPacketEscaped(bool f_packet_state)
{
    m_escaped = f_packet_state;
}

bool AOPacket::isPacketEscaped()
{
    return m_escaped;
}
