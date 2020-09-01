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
#ifdef _WIN32
#include <Windows.h>
#endif

#include <QCoreApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>
#include <QLibraryInfo>
#include <QSettings>

Advertiser* advertiser;
Server* server;

int main(int argc, char* argv[])
{
#ifdef _WIN32
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }
#endif
#ifdef __linux__
    // We have to do this before the QApplication is instantiated
    // As a result, we can't use QCommandLineParser
    for(int i = 0; i < argc; i++) {
        if(strcmp("-l", argv[i]) == 0 || strcmp("--headless", argv[i]) == 0){
            setenv("QT_QPA_PLATFORM", "minimal", 1);
        }
    }
#endif
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("akashi");
    QCoreApplication::setApplicationVersion("0.0.1");

    QCommandLineParser parser;
    parser.setApplicationDescription(
        app.translate("main", "A server for Attorney Online 2"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption verboseNetworkOption(
        QStringList() << "nv"
                      << "verbose-network",
        app.translate("main", "Write all network traffic to the console."));
    parser.addOption(verboseNetworkOption);

    parser.process(app);

    qDebug("Main application started");

    ConfigManager config_manager;
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
                                   settings.name, settings.description);
                advertiser->contactMasterServer();
            }

            // TODO: start the server here
            // TODO: send signal server starting.
            server = new Server(settings.port, settings.ws_port);
            server->start();
        }
    }

    return app.exec();
}
