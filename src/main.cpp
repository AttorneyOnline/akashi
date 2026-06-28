#include "core/console_log.h"
#include "core/server_context.h"
#include "softwareinformation.h"

#include <QCoreApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    akashi::installConsoleLog();
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(akashi::software::name);
    QCoreApplication::setApplicationVersion(akashi::software::fullVersion());

    qInfo().noquote() << "\n" + akashi::software::bootSplash();

    ServerContext context;
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &context, &ServerContext::shutdown);
    ExitCode code = context.start();
    if (code != ExitCode::Ok) {
        return static_cast<int>(code);
    }

    return app.exec();
}
