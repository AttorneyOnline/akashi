#include "core/console_input.h"

#include <QCoreApplication>
#include <QLocalSocket>

#include <cstdio>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace {

// The menu arrives as ANSI sequences; classic Windows consoles need
// telling before they render them.
void enableVtOutput()
{
#ifdef Q_OS_WIN
    HANDLE l_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD l_mode = 0;
    if (l_handle != INVALID_HANDLE_VALUE && GetConsoleMode(l_handle, &l_mode)) {
        SetConsoleMode(l_handle, l_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif
}

} // namespace

// Attaches this terminal to a running server's console menu over its
// local socket. Keys go one way, the menu comes back the other; ctrl+c
// detaches and leaves the server running. With stdin redirected, input
// drives the menu line by line instead, for scripted administration.
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QString l_socket_name = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("akashi-console");

    QLocalSocket l_socket;
    l_socket.connectToServer(l_socket_name);
    if (!l_socket.waitForConnected(3000)) {
        std::fprintf(stderr, "No server console at \"%s\": %s\n",
                     qPrintable(l_socket_name), qPrintable(l_socket.errorString()));
        std::fprintf(stderr, "Is the server running, and does its console_socket setting match?\n");
        return 1;
    }
    enableVtOutput();

    akashi::ConsoleInput l_input;
    l_input.setCaptureInterrupt(true);
    if (!l_input.isInteractive()) {
        l_socket.write(QByteArrayLiteral("lines\n"));
    }

    QObject::connect(&l_input, &akashi::ConsoleInput::keyPressed, &l_socket, [&l_socket](int f_key, QChar f_character) {
        switch (f_key) {
        case akashi::ConsoleInput::KeyUp:
            l_socket.write(QByteArrayLiteral("\x1b[A"));
            break;
        case akashi::ConsoleInput::KeyDown:
            l_socket.write(QByteArrayLiteral("\x1b[B"));
            break;
        case akashi::ConsoleInput::KeyEnter:
            l_socket.write(QByteArrayLiteral("\r"));
            break;
        case akashi::ConsoleInput::KeyBack:
            l_socket.write(QByteArrayLiteral("\x1b"));
            break;
        case akashi::ConsoleInput::KeyBackspace:
            l_socket.write(QByteArrayLiteral("\x7f"));
            break;
        case akashi::ConsoleInput::KeyCharacter:
            l_socket.write(QString(f_character).toUtf8());
            break;
        }
    });
    QObject::connect(&l_input, &akashi::ConsoleInput::lineEntered, &l_socket, [&l_socket](const QString &f_line) {
        l_socket.write(f_line.toUtf8() + '\n');
    });
    QObject::connect(&l_input, &akashi::ConsoleInput::interrupted, &app, [] {
        std::fputs("\nDetached; the server keeps running.\n", stdout);
        QCoreApplication::quit();
    });
    QObject::connect(&l_socket, &QLocalSocket::readyRead, &app, [&l_socket] {
        const QByteArray l_bytes = l_socket.readAll();
        std::fwrite(l_bytes.constData(), 1, l_bytes.size(), stdout);
        std::fflush(stdout);
    });
    QObject::connect(&l_socket, &QLocalSocket::disconnected, &app, [] {
        std::fputs("\nThe server closed the console connection.\n", stdout);
        QCoreApplication::quit();
    });

    l_input.start();
    return app.exec();
}
