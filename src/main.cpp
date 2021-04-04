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
#include "include/advertiser.h"
#include "include/server.h"
#include "include/config_manager.h"

#include <cstdlib>

#include <QCoreApplication>
#include <QDebug>

Advertiser* advertiser;
Server* server;

void cleanup() {
    server->deleteLater();
    advertiser->deleteLater();
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("akashi");
    QCoreApplication::setApplicationVersion("apricot r3 prerelease");
    std::atexit(cleanup);

    ConfigManager config_manager;
    if (config_manager.initConfig()) {
        // Config is sound, so proceed with starting the server
        // Validate some of the config before passing it on
        ConfigManager::server_settings settings;
        bool config_valid = config_manager.loadServerSettings(&settings);
        if (!config_valid) {
            qCritical() << "config.ini is invalid!";
            qCritical() << "Exiting server due to configuration issue.";
            exit(EXIT_FAILURE);
            QCoreApplication::quit();
        }

        else {
            if (settings.advertise_server) {
                advertiser =
                    new Advertiser(settings.ms_ip, settings.ms_port,
                                   settings.ws_port, settings.port,
                                   settings.name, settings.description);
                advertiser->contactMasterServer();
            }

            server = new Server(settings.port, settings.ws_port);
            server->start();
        }
    } else {
        qCritical() << "Exiting server due to configuration issue.";
        exit(EXIT_FAILURE);
        QCoreApplication::quit();
    }

    return app.exec();
}
