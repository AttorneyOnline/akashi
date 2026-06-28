#include "core/console_log.h"

#include <QString>
#include <QTime>

#include <cstdio>
#include <cstring>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {

bool s_color_enabled = false;

const char *levelTag(QtMsgType f_type)
{
    switch (f_type) {
    case QtDebugMsg:
        return "DBG";
    case QtInfoMsg:
        return "INF";
    case QtWarningMsg:
        return "WRN";
    case QtCriticalMsg:
        return "ERR";
    case QtFatalMsg:
        return "FTL";
    }
    return "???";
}

const char *levelColor(QtMsgType f_type)
{
    switch (f_type) {
    case QtDebugMsg:
        return "\x1b[90m"; // dim
    case QtInfoMsg:
        return "\x1b[36m"; // cyan
    case QtWarningMsg:
        return "\x1b[33m"; // yellow
    case QtCriticalMsg:
    case QtFatalMsg:
        return "\x1b[1;31m"; // bold red
    }
    return "";
}

void consoleMessageHandler(QtMsgType f_type, const QMessageLogContext &f_context, const QString &f_message)
{
    const char *l_dim = s_color_enabled ? "\x1b[90m" : "";
    const char *l_level_color = s_color_enabled ? levelColor(f_type) : "";
    const char *l_reset = s_color_enabled ? "\x1b[0m" : "";

    const QByteArray l_time = QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz")).toUtf8();

    // The category column, without the shared prefix; the default category
    // shows as blank.
    QString l_category_name;
    if (f_context.category && std::strcmp(f_context.category, "default") != 0) {
        l_category_name = QString::fromLatin1(f_context.category);
        if (l_category_name.startsWith(QStringLiteral("akashi."))) {
            l_category_name = l_category_name.mid(7);
        }
    }
    const QByteArray l_category = l_category_name.leftJustified(9).toUtf8();

    // Continuation lines of a multi-line message align under the first.
    const QStringList l_lines = f_message.split(QLatin1Char('\n'));
    QByteArray l_out;
    for (int i = 0; i < l_lines.size(); i++) {
        if (i == 0) {
            l_out += l_dim;
            l_out += l_time;
            l_out += l_reset;
            l_out += ' ';
            l_out += l_level_color;
            l_out += levelTag(f_type);
            l_out += l_reset;
            l_out += ' ';
            l_out += l_dim;
            l_out += l_category;
            l_out += l_reset;
            l_out += ' ';
        }
        else {
            // 12 time + 1 + 3 level + 1 + 9 category + 1 = 27 columns.
            l_out += QByteArray(27, ' ');
        }
        l_out += l_lines[i].toUtf8();
        l_out += '\n';
    }

    std::fputs(l_out.constData(), stderr);
    std::fflush(stderr);
}

} // namespace

namespace akashi {

void installConsoleLog()
{
#ifdef Q_OS_WIN
    // Classic consoles need virtual terminal sequences switched on; the
    // Windows Terminal already understands them.
    if (_isatty(_fileno(stderr))) {
        HANDLE l_handle = GetStdHandle(STD_ERROR_HANDLE);
        DWORD l_mode = 0;
        if (l_handle != INVALID_HANDLE_VALUE && GetConsoleMode(l_handle, &l_mode)) {
            s_color_enabled = SetConsoleMode(l_handle, l_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
        }
    }
#else
    s_color_enabled = isatty(fileno(stderr)) != 0;
#endif
    qInstallMessageHandler(consoleMessageHandler);
}

} // namespace akashi
