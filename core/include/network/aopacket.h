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

#include "include/packet/packet_info.h"
#include "include/area_data.h"
#include "include/aoclient.h"

class AOClient;

/**
 * @brief An Attorney Online 2 compatible packet.
 *
 * @see https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md for a general explanation
 * on Attorney Online 2's network protocol.
 */
class AOPacket
{
  public:
    AOPacket(QStringList p_contents);

    /**
     * @brief Destructor for the AOPacket
     */
    ~AOPacket(){};

    /**
     * @brief Returns the current content of the packet
     *
     * @return The content of the packet.
     */
    const QStringList getContent();

    /**
     * @brief Converts the header and content into a single string.
     *
     * @return String converted packet.
     */
    QString toString();

    /**
     * @brief Converts the entire packet, header and content, to a UTF8 formatted ByteArray.
     *
     * @return A UTF-8 representation of the packet.
     */
    QByteArray toUtf8();

    /**
     * @brief Allows editing of the content inside the packet on a per-field basis.
     */
    void setContentField(int f_content_index, QString f_content_data);

    /**
     * @brief Escapes the content of the packet using AO2's escape codes.
     *
     * @see https://github.com/AttorneyOnline/docs/blob/master/AO%20Documentation/docs/development/network.md#escape-codes
     */
    void escapeContent();

    /**
     * @brief Unescapes the content of the packet using AO2's escape codes.
     *
     * @see https://github.com/AttorneyOnline/docs/blob/master/AO%20Documentation/docs/development/network.md#escape-codes
     */
    void unescapeContent();

    /**
     * @brief Due to the way AO's netcode actively fights you, you have to do some specific considerations when escaping evidence.
     */
    void escapeEvidence();

    /**
     * @brief Sets the state if a packet has already been escaped or not.
     *
     * @details This is partially a workaround to make edge case behaviour possible while maintaining a
     * mostly unified escape/unescape path.
     *
     * @param Boolean value of the current state.
     *
     */
    void setPacketEscaped(bool f_packet_state);

    /**
     * @brief Returns if the packet is currently escaped or not.
     *
     * @details If a packet is escaped, it likely has either just been received by the server or is about to be written
     * to a network socket. There should **NEVER** be an instance where an unescaped packet is processed inside the server.
     *
     * @return If true, the packet is escaped. If false, it is unescaped and plain text.
     */
    bool isPacketEscaped();

    virtual PacketInfo getPacketInfo() const = 0;
    virtual void handlePacket(AreaData* area, AOClient& client) const = 0;
    virtual bool validatePacket() const = 0;

  protected:
    /**
     * @brief The contents of the packet.
     */
    QStringList m_content;

    /**
     * @brief Wether the packet is currently escaped or not. If false, the packet is unescaped.
     */
    bool m_escaped;

    /**
     * @brief According to AO documentation a complete packet is finished using the percent symbol.
     *
     * @details Note : This is due to AOs inability to determine the packet length, making it read forever otherwise.
     */
    const QString packetFinished = "%";
};

#endif // PACKET_MANAGER_H
