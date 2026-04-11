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
#include "network/aopacket.h"

#include "packet/packet_casea.h"
#include "packet/packet_ch.h"
#include "packet/packet_factory.h"
#include "packet/packet_hp.h"
#include "packet/packet_ma.h"
#include "packet/packet_pr.h"
#include "packet/packet_pw.h"
#include "packet/packet_zz.h"

AOPacket::AOPacket(QStringList p_contents) :
    m_content(p_contents),
    m_escaped(false)
{
}

AOPacket::~AOPacket() {}

const QStringList AOPacket::content()
{
    return m_content;
}

QString AOPacket::toString()
{
    if (!isPacketEscaped() && !(packetInfo().header == "LE")) {
        // We will never send unescaped data to a client, unless its evidence.
        this->escapeContent();
    }
    else {
        // Of course AO has SOME expection to the rule.
        this->escapeEvidence();
    }
    return QString("%1#%2#%3").arg(packetInfo().header, m_content.join("#"), packetFinished);
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

// The handshake family (HI, ID, askchaa, RC, RM, RD, CC), the chat family
// (CT, MS, DE, EE, SETCASE) and the area/music family (MC, RT, PE) live in
// the packet registry now; this list shrinks as the other families move over.
void AOPacket::registerPackets()
{
    PacketFactory::registerClass<PacketCasea>("CASEA");
    PacketFactory::registerClass<PacketCH>("CH");
    PacketFactory::registerClass<PacketHP>("HP");
    PacketFactory::registerClass<PacketPW>("PW");
    PacketFactory::registerClass<PacketMA>("MA");
    PacketFactory::registerClass<PacketZZ>("ZZ");
    PacketFactory::registerClass<PacketPR>("PR");
    PacketFactory::registerClass<PacketPU>("PU");
}
