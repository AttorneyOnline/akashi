#include "core/console_input.h"

#include <QMetaObject>

#include <iostream>
#include <string>
#include <thread>

#ifdef Q_OS_WIN
#include <conio.h>
#include <io.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace akashi {

namespace {

#ifndef Q_OS_WIN
termios s_saved_termios;
bool s_termios_saved = false;
#endif

bool stdinIsTerminal()
{
#ifdef Q_OS_WIN
    return _isatty(_fileno(stdin)) != 0;
#else
    return isatty(fileno(stdin)) != 0;
#endif
}

} // namespace

ConsoleInput::ConsoleInput(QObject *parent) :
    QObject(parent),
    m_alive(std::make_shared<std::atomic_bool>(true))
{
    m_interactive = stdinIsTerminal();
}

ConsoleInput::~ConsoleInput()
{
    *m_alive = false;
#ifndef Q_OS_WIN
    if (s_termios_saved) {
        tcsetattr(STDIN_FILENO, TCSANOW, &s_saved_termios);
    }
#endif
}

void ConsoleInput::start()
{
    if (m_interactive) {
        startKeyReader();
    }
    else {
        startLineReader();
    }
}

void ConsoleInput::startLineReader()
{
    auto l_alive = m_alive;
    // Detached on purpose: getline blocks with no portable way to interrupt
    // it, so the thread simply runs until stdin closes or the process ends.
    std::thread([this, l_alive] {
        std::string l_line;
        while (std::getline(std::cin, l_line)) {
            const QString l_text = QString::fromStdString(l_line).trimmed();
            if (!*l_alive) {
                return;
            }
            QMetaObject::invokeMethod(this, [this, l_text] { Q_EMIT lineEntered(l_text); }, Qt::QueuedConnection);
        }
    }).detach();
}

void ConsoleInput::startKeyReader()
{
    auto l_alive = m_alive;
    auto l_emit = [this, l_alive](int f_key, QChar f_character) {
        if (!*l_alive) {
            return false;
        }
        QMetaObject::invokeMethod(this, [this, f_key, f_character] { Q_EMIT keyPressed(f_key, f_character); }, Qt::QueuedConnection);
        return true;
    };
    auto l_interrupt = [this, l_alive] {
        if (*l_alive) {
            QMetaObject::invokeMethod(this, [this] { Q_EMIT interrupted(); }, Qt::QueuedConnection);
        }
    };
    const bool l_capture = m_capture_interrupt;

#ifdef Q_OS_WIN
    std::thread([l_emit, l_interrupt, l_capture] {
        for (;;) {
            const int l_first = _getch();
            if (l_first == 0 || l_first == 0xE0) {
                switch (_getch()) {
                case 72: // up
                    if (!l_emit(KeyUp, {})) return;
                    break;
                case 80: // down
                    if (!l_emit(KeyDown, {})) return;
                    break;
                case 75: // left
                    if (!l_emit(KeyBack, {})) return;
                    break;
                case 77: // right enters, like most terminal menus
                    if (!l_emit(KeyEnter, {})) return;
                    break;
                }
            }
            else if (l_first == '\r' || l_first == '\n') {
                if (!l_emit(KeyEnter, {})) return;
            }
            else if (l_first == 27) {
                if (!l_emit(KeyBack, {})) return;
            }
            else if (l_first == 8) {
                if (!l_emit(KeyBackspace, {})) return;
            }
            else if (l_first == 3) { // ctrl+c
                if (l_capture) {
                    l_interrupt();
                }
                return;
            }
            else if (l_first >= 32 && l_first < 127) {
                if (!l_emit(KeyCharacter, QChar(l_first))) return;
            }
        }
    }).detach();
#else
    // Raw mode: keys arrive one by one, unechoed; restored on destruction.
    termios l_raw;
    if (tcgetattr(STDIN_FILENO, &s_saved_termios) == 0) {
        s_termios_saved = true;
        l_raw = s_saved_termios;
        l_raw.c_lflag &= ~(ICANON | ECHO);
        // Capturing the interrupt needs ctrl+c delivered as a byte, not a signal.
        if (m_capture_interrupt) {
            l_raw.c_lflag &= ~ISIG;
        }
        l_raw.c_cc[VMIN] = 1;
        l_raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &l_raw);
    }

    std::thread([l_emit, l_interrupt, l_capture] {
        char l_byte = 0;
        while (read(STDIN_FILENO, &l_byte, 1) == 1) {
            if (l_byte == 3) { // ctrl+c
                if (l_capture) {
                    l_interrupt();
                }
                return;
            }
            if (l_byte == 27) {
                char l_bracket = 0, l_code = 0;
                if (read(STDIN_FILENO, &l_bracket, 1) == 1 && l_bracket == '[' && read(STDIN_FILENO, &l_code, 1) == 1) {
                    switch (l_code) {
                    case 'A':
                        if (!l_emit(KeyUp, {})) return;
                        break;
                    case 'B':
                        if (!l_emit(KeyDown, {})) return;
                        break;
                    case 'D':
                        if (!l_emit(KeyBack, {})) return;
                        break;
                    case 'C':
                        if (!l_emit(KeyEnter, {})) return;
                        break;
                    }
                }
                else {
                    if (!l_emit(KeyBack, {})) return;
                }
            }
            else if (l_byte == '\r' || l_byte == '\n') {
                if (!l_emit(KeyEnter, {})) return;
            }
            else if (l_byte == 127 || l_byte == 8) {
                if (!l_emit(KeyBackspace, {})) return;
            }
            else if (l_byte >= 32 && l_byte < 127) {
                if (!l_emit(KeyCharacter, QChar(l_byte))) return;
            }
        }
    }).detach();
#endif
}

} // namespace akashi
