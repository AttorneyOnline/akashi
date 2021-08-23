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

class MessageLog {
public:

    explicit MessageLog();
    struct m_content {
      QString charname;
      QString oocname;
      int charID;
      QString IPID;
      QString HDID;
      QString message;
      AreaData* area;
    };
};

class ModerativeLog {
public:
    explicit ModerativeLog();
    struct m_content {
        QString moderatorName;
        QString ipid;
        QString hdid;
        QString targetName;
        QString targetOOCName;
        AreaData* area;
    };
};

#endif // U_LOGGER_DATATYPES_H
