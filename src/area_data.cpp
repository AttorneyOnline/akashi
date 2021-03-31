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

AreaData::AreaData(QString p_name, int p_index) :
    index(p_index),
    player_count(0),
    status(IDLE),
    locked(FREE),
    document("No document."),
    def_hp(10),
    pro_hp(10)
{
    QStringList name_split = p_name.split(":");
    name_split.removeFirst();
    name = name_split.join(":");
    QSettings areas_ini("config/areas.ini", QSettings::IniFormat);
    areas_ini.beginGroup(p_name);
    background = areas_ini.value("background", "gs4").toString();
    is_protected = areas_ini.value("protected_area", "false").toBool();
    iniswap_allowed = areas_ini.value("iniswap_allowed", "true").toBool();
    bg_locked = areas_ini.value("bg_locked", "false").toBool();
    QString configured_evi_mod = areas_ini.value("evidence_mod", "FFA").toString().toLower();
    areas_ini.endGroup();
    QSettings config_ini("config/config.ini", QSettings::IniFormat);
    config_ini.beginGroup("Options");
    int log_size = config_ini.value("logbuffer", 50).toInt();
    config_ini.endGroup();
    if (log_size == 0)
        log_size = 500;
    logger = new Logger(log_size, this);
    QTimer* timer1 = new QTimer();
    timers.append(timer1);
    QTimer* timer2 = new QTimer();
    timers.append(timer2);
    QTimer* timer3 = new QTimer();
    timers.append(timer3);
    QTimer* timer4 = new QTimer();
    timers.append(timer4);

    if (configured_evi_mod == "cm")
        evi_mod = EvidenceMod::CM;
    else if (configured_evi_mod == "mod")
        evi_mod = EvidenceMod::MOD;
    else if (configured_evi_mod == "hiddencm")
        evi_mod = EvidenceMod::HIDDEN_CM;
    else
        evi_mod = EvidenceMod::FFA;
}
