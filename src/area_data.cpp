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
#include "include/area_data.h"

AreaData::AreaData(QStringList characters, QString p_name, int p_index)
{
    name = p_name;
    index = p_index;
    for (QString cur_char : characters) {
        characters_taken.insert(cur_char, false);
    }
    QSettings areas_ini("areas.ini", QSettings::IniFormat);
    areas_ini.beginGroup(p_name);
    background = areas_ini.value("background", "gs4").toString();
    areas_ini.endGroup();
    player_count = 0;
    current_cm = "FREE";
    locked = false;
    status = "FREE";
    def_hp = 10;
    pro_hp = 10;
}
