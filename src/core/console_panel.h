#pragma once

#include "akashi_core_export.h"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringDecoder>

class ServerContext;
class QLocalServer;
class QLocalSocket;
class QTimer;

namespace akashi {

class ConsoleMenu;

// Serves the console menu over a local socket - a named pipe on Windows,
// a socket file on Linux - so an operator on the same machine can attach
// a terminal with the akashi-console client even when the server runs as
// a service without one. Each connection gets its own menu session, and
// the socket only admits the user the server runs as.
class AKASHI_CORE_EXPORT ConsolePanelServer : public QObject
{
    Q_OBJECT

  public:
    explicit ConsolePanelServer(ServerContext *f_server, QObject *parent = nullptr);

    // Starts listening under the given socket name; empty leaves the
    // panel off. Returns whether the socket opened.
    bool listen(const QString &f_socket_name);

  private:
    void acceptConnection();

    ServerContext *m_server;
    QLocalServer *m_socket_server = nullptr;
};

// One attached operator: turns socket bytes into menu keys or lines and
// menu output back into socket bytes. A connection whose first bytes are
// the line "lines" runs in line mode for scripted use; anything else is a
// terminal sending keys, arrows arriving as ANSI sequences.
class ConsolePanelSession : public QObject
{
    Q_OBJECT

  public:
    ConsolePanelSession(QLocalSocket *f_socket, ConsoleMenu *f_menu_service, QObject *parent = nullptr);

  private Q_SLOTS:
    // Turns a lone escape into KeyBack once no arrow sequence followed it.
    void onEscapeTimeout();

  private:
    enum class Mode
    {
        Undecided,
        Keys,
        Lines,
    };

    void readSocket();
    void processKeys();
    void processLines();
    // The session's console sink: rendered menu bytes go to the pipe socket.
    void writeToSocket(const QByteArray &f_bytes);
    void sendCharacters(const QByteArray &f_bytes);

    QLocalSocket *m_socket;
    ConsoleMenu *m_menu = nullptr;
    QTimer *m_escape_timer;
    QStringDecoder m_decoder{QStringDecoder::Utf8};
    Mode m_mode = Mode::Undecided;
    QByteArray m_pending;
    bool m_skip_linefeed = false;
};

} // namespace akashi
