#include <QCoreApplication>

#include "serviceregistry.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("akashi");
    QCoreApplication::setApplicationVersion("jackfruit (1.9)");

    ServiceRegistry *registry = new ServiceRegistry(&app);

    return app.exec();
}
