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
#ifndef AKASHIMAIN_H
#define AKASHIMAIN_H

#include "include/advertiser.h"
#include "include/config_manager.h"
#include "include/server.h"

#include <QDebug>
#include <QMainWindow>
#include <QSettings>

QT_BEGIN_NAMESPACE
namespace Ui {
class AkashiMain;
}
QT_END_NAMESPACE

class AkashiMain : public QMainWindow {
    Q_OBJECT

  public:
    AkashiMain(QWidget* parent = nullptr);
    ~AkashiMain();

    ConfigManager config_manager;

    void generateDefaultConfig(bool backup_old);
    void updateConfig(int current_version);

  private:
    Ui::AkashiMain* ui;
    Advertiser* advertiser;
    Server* server;
};
#endif // AKASHIMAIN_H
