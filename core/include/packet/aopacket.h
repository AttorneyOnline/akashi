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

#include "include/area_data.h"
#include "include/acl_mask.h"

/**
 * @brief An Attorney Online 2 compatible packet.
 *
 * @see https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md for a general explanation
 * on Attorney Online 2's network protocol.
 */
class AOPacket {
  public:
    /**
     * @brief Creates an AOPacket with the given header and contents.
     *
     * @param p_header The header for the packet.
     * @param p_contents The contents of the packet.
     */
    AOPacket(QStringList p_contents);

    /**
     * @brief Returns the string representation of the packet.
     *
     * @return See brief description.
     */
    QString toString();

    /**
     * @brief Convenience function over AOPacket::toString() + QString::toUtf8().
     *
     * @return A UTF-8 representation of the packet.
     */
    QByteArray toUtf8();

    QString getHeader() { return header; };
    QStringList getContents() { return contents; };

    unsigned long long getAclMask() { return acl_mask; };
    int getMinArgs() { return min_args; };

    virtual void handlePacket(AreaData* area, AOClient& client) const = 0;
    virtual bool validatePacket() const = 0;

  protected:
    /**
     * @brief The string that indentifies the type of the packet.
     */
    QString header;

    /**
     * @brief The list of parameters for the packet. Can be empty.
     */
    QStringList contents;

    unsigned long long acl_mask;
    int min_args;
};

#endif // PACKET_MANAGER_H
