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
#include "config_manager.h"
#include "server.h"

#include <cstdlib>

#include <QCoreApplication>
#include <QDebug>

Server *server;

void cleanup()
{
    server->deleteLater();
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("akashi");
    QCoreApplication::setApplicationVersion("honeydew hotfix (1.8.1)");
    std::atexit(cleanup);

    // Verify server configuration is sound.
    if (!ConfigManager::verifyServerConfig()) {
        qCritical() << "config.ini is invalid!";
        qCritical() << "Exiting server due to configuration issue.";
        exit(EXIT_FAILURE);
        QCoreApplication::quit();
    }
    else {
        server = new Server(ConfigManager::serverPort());
        server->start();
    }

    return app.exec();
}
