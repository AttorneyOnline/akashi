#include "akashi/logging_categories.h"
#include "core/console_input.h"
#include "core/console_log.h"
#include "core/console_menu.h"
#include "core/console_panel.h"
#include "core/server_context.h"
#include "core/server_settings.h"
#include "softwareinformation.h"

#include <QCoreApplication>
#include <QDebug>
#include <QTextStream>

// The --check-config problem counter: every warning or worse out of the
// config and command-extension categories during the dry run is a
// pre-flight failure. The messages still print through the console log.
static QtMessageHandler s_forward_handler = nullptr;
static int s_config_problems = 0;
static void countConfigProblems(QtMsgType f_type, const QMessageLogContext &f_context, const QString &f_message)
{
    if ((f_type == QtWarningMsg || f_type == QtCriticalMsg) && f_context.category) {
        const QLatin1String l_category(f_context.category);
        if (l_category == QLatin1String("akashi.config") || l_category == QLatin1String("akashi.commands")) {
            s_config_problems++;
        }
    }
    if (s_forward_handler) {
        s_forward_handler(f_type, f_context, f_message);
    }
}

int main(int argc, char *argv[])
{
    akashi::installConsoleLog();
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(akashi::software::name);
    QCoreApplication::setApplicationVersion(akashi::software::fullVersion());

    // --check-config: the cold-boot dry run. The whole start path runs -
    // conversions, roles, world, plugins, grants - with the socket and
    // advertiser skipped; the compiled offers print for diffing and any
    // config problem makes the exit code speak.
    const bool l_check_only = QCoreApplication::arguments().contains(QStringLiteral("--check-config"));
    if (l_check_only) {
        s_forward_handler = qInstallMessageHandler(&countConfigProblems);
    }

    qCInfo(akashiServer).noquote() << "\n" + akashi::software::bootSplash();

    ServerContext context;
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &context, &ServerContext::shutdown);
    ExitCode code = context.start();
    if (code != ExitCode::Ok) {
        return static_cast<int>(code);
    }

    if (l_check_only) {
        context.printCompiledOffers();
        QTextStream l_out(stdout);
        if (s_config_problems > 0) {
            l_out << "--check-config: " << s_config_problems << " problem(s) found - see the warnings above.\n";
        }
        else {
            l_out << "--check-config: clean.\n";
        }
        l_out.flush();
        context.shutdown();
        return s_config_problems > 0 ? static_cast<int>(ExitCode::InvalidConfig) : static_cast<int>(ExitCode::Ok);
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
