#include <QCoreApplication>
#include <QDebug>

#include "server.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("akashi");
    QCoreApplication::setApplicationVersion("jackfruit (1.9)");

    qDebug() << "Starting application.";

    Server l_server(&app);
    QObject::connect(&l_server,
                     &Server::shutdownRequested,
                     &app,
                     &QCoreApplication::quit,
                     Qt::QueuedConnection);

    return app.exec();
}
