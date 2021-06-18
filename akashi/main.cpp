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
#include <include/advertiser.h>
#include <include/server.h>
#include <include/config_manager.h>

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
    QCoreApplication::setApplicationVersion("banana");
    std::atexit(cleanup);

    if (ConfigManager::verifyServerConfig()) {
        // Config is sound, so proceed with starting the server
        // Validate some of the config before passing it on
        bool config_valid = ConfigManager::loadConfigSettings();
        if (!config_valid) {
            qCritical() << "config.ini is invalid!";
            qCritical() << "Exiting server due to configuration issue.";
            exit(EXIT_FAILURE);
            QCoreApplication::quit();
        }

        else {
            if (ConfigManager::advertiseServer()) {
                advertiser =
                    new Advertiser(ConfigManager::masterServerIP(), ConfigManager::masterServerPort(),
                                   ConfigManager::webaoPort(), ConfigManager::serverPort(),
                                   ConfigManager::serverName(), ConfigManager::serverDescription());
                advertiser->contactMasterServer();
            }

            server = new Server(ConfigManager::serverPort(), ConfigManager::webaoPort());

            if (advertiser != nullptr) {
                QObject::connect(server, &Server::reloadRequest, advertiser, &Advertiser::reloadRequested);
            }
            server->start();
        }
    } else {
        qCritical() << "Exiting server due to configuration issue.";
        exit(EXIT_FAILURE);
        QCoreApplication::quit();
    }

    return app.exec();
}
