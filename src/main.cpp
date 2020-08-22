#include "include/akashimain.h"

#include <QApplication>
#include <QDebug>
#include <QCommandLineParser>
#include <QCommandLineOption>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("akashi");
    QApplication::setApplicationVersion("0.0.1");

    QCommandLineParser parser;
    parser.setApplicationDescription("A server for Attorney Online 2");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption headlessOption(QStringList() << "l" << "headless", "Run the server without a GUI");
    parser.addOption(headlessOption);

    parser.process(app);
    bool headless = parser.isSet(headlessOption);

    AkashiMain w;
    if(!headless)
        w.show();

    return app.exec();
}
