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

#ifdef _WIN32
#include <Windows.h>
#endif

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>
#include <QLibraryInfo>
#include <QSettings>
#include <QTranslator>

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
    for(int i; i < argc; i++) {
        if(strcmp("-l", argv[i]) == 0 || strcmp("--headless", argv[i]) == 0){
            setenv("QT_QPA_PLATFORM", "minimal");
        }
    }
#endif
    QApplication app(argc, argv);
    QApplication::setApplicationName("akashi");
    QApplication::setApplicationVersion("0.0.1");

    QSettings config("config.ini", QSettings::IniFormat);
    config.beginGroup("Options");
    QString language =
        config.value("language", QLocale().bcp47Name()).toString();

    QTranslator qt_translator;
    qt_translator.load("qt_" + language,
                       QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    app.installTranslator(&qt_translator);

    QTranslator translator;
    translator.load("akashi_" + language, ":/resource/translation/");
    app.installTranslator(&translator);

    QCommandLineParser parser;
    parser.setApplicationDescription(
        app.translate("main", "A server for Attorney Online 2"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption headlessOption(
        QStringList() << "l"
                      << "headless",
        app.translate("main", "Run the server without a GUI."));
    QCommandLineOption verboseNetworkOption(
        QStringList() << "nv"
                      << "verbose-network",
        app.translate("main", "Write all network traffic to the console."));
    parser.addOption(headlessOption);
    parser.addOption(verboseNetworkOption);

    parser.process(app);
    bool headless = parser.isSet(headlessOption);

    AkashiMain w;
    if (!headless)
        w.show();

    return app.exec();
}
