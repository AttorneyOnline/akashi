#include "world/floodguard.h"

namespace akashi {

Floodguard::Floodguard(QObject *parent) :
    QObject(parent)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &Floodguard::reopen);
}

void Floodguard::start(int f_duration_msecs)
{
    m_allowed = false;
    m_timer.start(f_duration_msecs);
}

} // namespace akashi
