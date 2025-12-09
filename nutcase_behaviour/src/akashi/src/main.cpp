#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("akashi");
    QCoreApplication::setApplicationVersion("jackfruit (1.9)");

    return app.exec();
}
