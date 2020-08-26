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
#include "include/akashimain.h"
#include "ui_akashimain.h"

AkashiMain::AkashiMain(QWidget* parent)
    : QMainWindow(parent), config_manager(), ui(new Ui::AkashiMain)
{
    ui->setupUi(this);
    qDebug("Main application started");

    if (config_manager.initConfig()) {
        // Config is sound, so proceed with starting the server
        // Validate some of the config before passing it on
        ConfigManager::server_settings settings;
        bool config_valid = config_manager.loadServerSettings(&settings);

        if (!config_valid) {
            // TODO: send signal config invalid
            config_manager.generateDefaultConfig(true);
        }
        else {
            if (settings.advertise_server) {
                // TODO: send signal advertiser started
                advertiser =
                    new Advertiser(settings.ms_ip, settings.port,
                                   settings.ws_port, settings.local_port,
                                   settings.name, settings.description, this);
                advertiser->contactMasterServer();
            }

            // TODO: start the server here
            // TODO: send signal server starting.
            server = new Server(settings.port, settings.ws_port);
            server->start();
        }
    }
}

AkashiMain::~AkashiMain()
{
    delete ui;
    delete advertiser;
    delete server;
}
