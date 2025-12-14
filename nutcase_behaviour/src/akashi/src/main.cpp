#include <QCoreApplication>
#include <QDebug>

#include "server.h"
#include "softwareinformation.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(SoftwareInfo::NAME);
    QCoreApplication::setApplicationVersion(SoftwareInfo::VERSION);

    qDebug() << "Starting" << SoftwareInfo::DISPLAY_NAME << "version" << SoftwareInfo::VERSION
             << SoftwareInfo::CODENAME;
    qDebug() << "Built:" << SoftwareInfo::BUILD_DATE << "(" << SoftwareInfo::BUILD_TYPE << ")";
    qDebug() << "Qt:" << QT_VERSION_STR << "| Commit:" << SoftwareInfo::GIT_COMMIT
             << "| Branch:" << SoftwareInfo::GIT_BRANCH;
    qDebug().noquote() << "MOTB:" << SoftwareInfo::getRandomSplash();

    Server l_server(&app);
    QObject::connect(&l_server,
                     &Server::shutdownRequested,
                     &app,
                     &QCoreApplication::quit,
                     Qt::QueuedConnection);

    return app.exec();
}
