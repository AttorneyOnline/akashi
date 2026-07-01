#include "core/console_panel.h"

#include "core/console_input.h"
#include "core/console_menu.h"
#include "core/logging_categories.h"
#include "core/server_context.h"

#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>

namespace akashi {

ConsolePanelServer::ConsolePanelServer(ServerContext *f_server, QObject *parent) :
    QObject(parent),
    m_server(f_server)
{}

bool ConsolePanelServer::listen(const QString &f_socket_name)
{
    if (f_socket_name.isEmpty()) {
        return false;
    }
    m_socket_server = new QLocalServer(this);
    m_socket_server->setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_socket_server->listen(f_socket_name)) {
        // A crashed run can leave the socket file behind; clear it and retry.
        QLocalServer::removeServer(f_socket_name);
        if (!m_socket_server->listen(f_socket_name)) {
            qCWarning(akashiConsole).noquote() << "The console socket" << f_socket_name << "did not open:" << m_socket_server->errorString();
            return false;
        }
    }
    connect(m_socket_server, &QLocalServer::newConnection, this, &ConsolePanelServer::acceptConnection);
    qCInfo(akashiConsole).noquote() << "Console attachable at" << m_socket_server->fullServerName() << "(akashi-console)";
    return true;
}

void ConsolePanelServer::acceptConnection()
{
    while (QLocalSocket *l_socket = m_socket_server->nextPendingConnection()) {
        new ConsolePanelSession(l_socket, m_server->consoleMenu(), this);
    }
}

ConsolePanelSession::ConsolePanelSession(QLocalSocket *f_socket, ConsoleMenu *f_menu_service, QObject *parent) :
    QObject(parent),
    m_socket(f_socket),
    m_escape_timer(new QTimer(this))
{
    m_socket->setParent(this);
    m_menu = f_menu_service->createSession(this);
    m_menu->setInteractive(true, true);
    m_menu->setSink([this](const QByteArray &f_bytes) { m_socket->write(f_bytes); });

    connect(m_socket, &QLocalSocket::readyRead, this, &ConsolePanelSession::readSocket);
    connect(m_socket, &QLocalSocket::disconnected, this, &QObject::deleteLater);

    // A lone escape needs a moment to prove no arrow sequence follows it.
    m_escape_timer->setSingleShot(true);
    m_escape_timer->setInterval(50);
    connect(m_escape_timer, &QTimer::timeout, this, [this] {
        if (m_mode == Mode::Keys && m_pending == QByteArrayLiteral("\x1b")) {
            m_pending.clear();
            m_menu->handleKey(ConsoleInput::KeyBack, {});
        }
    });

    m_menu->show();
}

void ConsolePanelSession::readSocket()
{
    m_pending += m_socket->readAll();
    if (m_mode == Mode::Undecided) {
        // The first bytes pick the mode: scripts announce "lines", a
        // terminal just starts sending keys.
        const QByteArray l_announcement = QByteArrayLiteral("lines\r\n");
        if (m_pending.startsWith(QByteArrayLiteral("lines\n")) || m_pending.startsWith(l_announcement)) {
            m_pending.remove(0, m_pending.indexOf('\n') + 1);
            m_mode = Mode::Lines;
            m_menu->setInteractive(false, false);
            m_menu->show();
        }
        else if (l_announcement.startsWith(m_pending)) {
            return; // could still become the announcement; wait for more
        }
        else {
            m_mode = Mode::Keys;
        }
    }
    if (m_mode == Mode::Keys) {
        processKeys();
    }
    else {
        processLines();
    }
}

void ConsolePanelSession::processLines()
{
    int l_newline = -1;
    while ((l_newline = m_pending.indexOf('\n')) >= 0) {
        QByteArray l_line = m_pending.left(l_newline);
        m_pending.remove(0, l_newline + 1);
        if (l_line.endsWith('\r')) {
            l_line.chop(1);
        }
        m_menu->handleLine(QString::fromUtf8(l_line).trimmed());
    }
}

void ConsolePanelSession::processKeys()
{
    while (!m_pending.isEmpty()) {
        const unsigned char l_byte = static_cast<unsigned char>(m_pending.at(0));
        if (l_byte != '\n') {
            m_skip_linefeed = false;
        }
        if (l_byte == 0x1b) {
            if (m_pending.size() == 1) {
                m_escape_timer->start();
                return;
            }
            if (m_pending.at(1) == '[') {
                // An ANSI sequence runs to its final byte in the @ to ~ range.
                int l_end = 2;
                while (l_end < m_pending.size()) {
                    const unsigned char l_candidate = static_cast<unsigned char>(m_pending.at(l_end));
                    if (l_candidate >= 0x40 && l_candidate <= 0x7e) {
                        break;
                    }
                    l_end++;
                }
                if (l_end >= m_pending.size()) {
                    if (m_pending.size() > 16) {
                        m_pending.clear(); // an unterminated sequence is garbage
                    }
                    return;
                }
                const char l_code = m_pending.at(l_end);
                m_pending.remove(0, l_end + 1);
                switch (l_code) {
                case 'A':
                    m_menu->handleKey(ConsoleInput::KeyUp, {});
                    break;
                case 'B':
                    m_menu->handleKey(ConsoleInput::KeyDown, {});
                    break;
                case 'C':
                    m_menu->handleKey(ConsoleInput::KeyEnter, {});
                    break;
                case 'D':
                    m_menu->handleKey(ConsoleInput::KeyBack, {});
                    break;
                default:
                    break; // other sequences have no menu meaning
                }
                continue;
            }
            m_pending.remove(0, 1);
            m_menu->handleKey(ConsoleInput::KeyBack, {});
            continue;
        }
        if (l_byte == '\r') {
            m_pending.remove(0, 1);
            m_skip_linefeed = true; // a CRLF pair is one enter
            m_menu->handleKey(ConsoleInput::KeyEnter, {});
            continue;
        }
        if (l_byte == '\n') {
            m_pending.remove(0, 1);
            if (!m_skip_linefeed) {
                m_menu->handleKey(ConsoleInput::KeyEnter, {});
            }
            m_skip_linefeed = false;
            continue;
        }
        if (l_byte == 0x7f || l_byte == 0x08) {
            m_pending.remove(0, 1);
            m_menu->handleKey(ConsoleInput::KeyBackspace, {});
            continue;
        }
        if (l_byte == 0x03) { // ctrl+c detaches
            m_pending.clear();
            m_socket->disconnectFromServer();
            return;
        }
        if (l_byte < 0x20) {
            m_pending.remove(0, 1); // other control bytes have no menu meaning
            continue;
        }
        // A run of text bytes becomes characters.
        int l_end = 1;
        while (l_end < m_pending.size()) {
            const unsigned char l_candidate = static_cast<unsigned char>(m_pending.at(l_end));
            if (l_candidate < 0x20 || l_candidate == 0x7f) {
                break;
            }
            l_end++;
        }
        sendCharacters(m_pending.left(l_end));
        m_pending.remove(0, l_end);
    }
}

void ConsolePanelSession::sendCharacters(const QByteArray &f_bytes)
{
    // The decoder keeps its state, so a character split across reads
    // completes on the next call.
    const QString l_decoded = m_decoder(f_bytes);
    for (const QChar &l_character : l_decoded) {
        m_menu->handleKey(ConsoleInput::KeyCharacter, l_character);
    }
}

} // namespace akashi
