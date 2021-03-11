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
#ifndef AREA_DATA_H
#define AREA_DATA_H

#include "include/logger.h"

#include <QMap>
#include <QString>
#include <QSettings>
#include <QDebug>
#include <QTimer>
#include <QElapsedTimer>

class Logger;
class AreaData : public QObject {
  Q_OBJECT
  public:
    AreaData(QStringList p_characters, QString p_name, int p_index);

    struct Evidence {
        QString name;
        QString description;
        QString image;
    };
    QList<QTimer*> timers;
    QString name;
    int index;
    QMap<QString, bool> characters_taken;
    QList<Evidence> evidence;
    int player_count;
    enum Status {
      IDLE,
      RP,
      CASING,
      LOOKING_FOR_PLAYERS,
      RECESS,
      GAMING
    };
    Q_ENUM(Status);
    Status status;
    QList<int> owners;
    QList<int> invited;
    enum LockStatus {
      FREE,
      LOCKED,
      SPECTATABLE
    };
    Q_ENUM(LockStatus);
    LockStatus locked;
    QString background;
    bool is_protected;
    bool showname_allowed;
    bool iniswap_allowed;
    bool bg_locked;
    QString document;
    int def_hp;
    int pro_hp;
    QString current_music;
    QString music_played_by;
    Logger* logger;
};

#endif // AREA_DATA_H
