#pragma once

#include "akashi_core_export.h"

#include <QObject>
#include <QTimer>

namespace akashi {

// The IC message rate gate: start() closes it and the timer reopens it
// after the duration. The area and the whole server each hold one; a
// message must pass both.
class AKASHI_CORE_EXPORT Floodguard : public QObject
{
    Q_OBJECT

  public:
    explicit Floodguard(QObject *parent = nullptr);

    bool isMessageAllowed() const { return m_allowed; }

    void start(int f_duration_msecs);

  private Q_SLOTS:
    // The timer ran out; messages may pass again.
    void reopen() { m_allowed = true; }

  private:
    QTimer m_timer;
    bool m_allowed = true;
};

} // namespace akashi
