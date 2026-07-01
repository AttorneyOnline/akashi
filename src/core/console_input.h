#pragma once

#include "akashi_core_export.h"

#include <QObject>
#include <QString>

#include <atomic>
#include <memory>

namespace akashi {

// Reads the operator's keyboard on the server's own terminal without
// blocking the event loop. On a live terminal it reads raw keys, so menus
// navigate with the arrow keys; with stdin redirected it falls back to
// whole lines. The reader thread lives until stdin closes or the process
// ends; a server run without a terminal ends it quietly.
class AKASHI_CORE_EXPORT ConsoleInput : public QObject
{
    Q_OBJECT

  public:
    enum Key
    {
        KeyUp,
        KeyDown,
        KeyEnter,
        KeyBack,      // escape or the left arrow
        KeyBackspace,
        KeyCharacter,
    };

    explicit ConsoleInput(QObject *parent = nullptr);
    ~ConsoleInput() override;

    void start();

    // True when raw keys are available; false means lines only.
    bool isInteractive() const { return m_interactive; }

    // Reports ctrl+c through interrupted() instead of letting it end the
    // process. The attach client uses this to detach cleanly; the server
    // itself keeps the usual ctrl+c behavior. Call before start().
    void setCaptureInterrupt(bool f_capture) { m_capture_interrupt = f_capture; }

  Q_SIGNALS:
    void keyPressed(int f_key, QChar f_character);
    void lineEntered(const QString &f_line);
    void interrupted();

  private:
    void startLineReader();
    void startKeyReader();

    bool m_interactive = false;
    bool m_capture_interrupt = false;
    std::shared_ptr<std::atomic_bool> m_alive;
};

} // namespace akashi
