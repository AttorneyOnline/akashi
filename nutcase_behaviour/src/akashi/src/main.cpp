#include <QCoreApplication>

#include "pluginmanager.h"
#include "serviceregistry.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("akashi");
    QCoreApplication::setApplicationVersion("jackfruit (1.9)");

    ServiceRegistry *registry = new ServiceRegistry(&app);
    PluginManager *plug_man = new PluginManager(registry, &app);
    plug_man->loadPluginsFromDirectory();

    return app.exec();
}
