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
#ifndef U_LOGGER_DATATYPES_H
#define U_LOGGER_DATATYPES_H

#include <QString>
#include "include/area_data.h"

/**
 * @brief Convenience class to transport IC and OOC messages to the logger.
 */
class MessageLog {
public:
    explicit MessageLog();
    struct m_content {
      QString charName;
      QString oocName;
      QString ipid;
      QString hdid;
      QString message;
      AreaData* area;
    };
    m_content content;
};

/**
 * @brief Convenience class to transport information of moderator actions to the logger
 *
 * @details The only two moderator commands who take advantage of this are ban and kick.
 */
class ModerativeLog {
public:
    explicit ModerativeLog();
    struct m_content {
        QString moderatorName;
        QString ipid;
        QString hdid;
        QString targetName;
        QString targetOOCName;
        QString targetIPID;
        QString targetHDID;
        AreaData* area;
    };
    m_content content;
};

/**
 * @brief Convenience class to transport command usage information to the logger.
 */
class CommandLog {
public:
    explicit CommandLog();
    struct m_content {
        QString charName;
        QString oocName;
        QString ipid;
        QString hdid;
        QString command;
        QString cmdArgs;
        AreaData* area;
    };
    m_content content;
};

/**
 * @brief Convenience class to transport login attempt information to the logger.
 */
class LoginLog {
public:
    explicit LoginLog();
    struct m_content {
        QString charName;
        QString oocName;
        QString ipid;
        QString hdid;
        bool success;
        QString modname;
    };
    m_content content;
};

/**
 * @brief Convenience class to transport connection event information to the logger.
 */
class ConnectionLog {
public:
    explicit ConnectionLog();
    struct m_content {
        QString ip_address;
        QString hdid;
        QString ipid;
        bool success;
    };
    m_content content;
};

#endif // U_LOGGER_DATATYPES_H
