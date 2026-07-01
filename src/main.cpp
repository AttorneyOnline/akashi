#include "core/console_input.h"
#include "core/console_log.h"
#include "core/console_menu.h"
#include "core/console_panel.h"
#include "core/server_context.h"
#include "core/server_settings.h"
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

    // The menu is also attachable over a local socket, for servers running
    // without a terminal of their own.
    akashi::ConsolePanelServer panel(&context);
    panel.listen(context.serverSettings()->console_socket());

    // The operator menu on the server's own terminal.
    akashi::ConsoleInput console;
    context.consoleMenu()->setInteractive(console.isInteractive());
    QObject::connect(&console, &akashi::ConsoleInput::keyPressed,
                     context.consoleMenu(), &akashi::ConsoleMenu::handleKey);
    QObject::connect(&console, &akashi::ConsoleInput::lineEntered,
                     context.consoleMenu(), &akashi::ConsoleMenu::handleLine);
    console.start();
    context.consoleMenu()->show();

    return app.exec();
}
