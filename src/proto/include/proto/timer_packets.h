#pragma once

#include "proto/ao2_protocol.h"
#include "proto/packet.h"

#include <QString>

namespace akashi {

// The TI timer packet in its three shapes. Every timer a client sees is
// built here, whichever door set it: the join delivery, the send_timers
// rule action and /timer.

inline Packet timerShow(int f_timer_id)
{
    return Packet(ao2::HEADER_TI, {QString::number(f_timer_id), QStringLiteral("2")});
}

inline Packet timerHide(int f_timer_id)
{
    return Packet(ao2::HEADER_TI, {QString::number(f_timer_id), QStringLiteral("3")});
}

// A running timer counts down from the value; a paused one stands still on it.
inline Packet timerValue(int f_timer_id, int f_msecs, bool f_paused = false)
{
    return Packet(ao2::HEADER_TI, {QString::number(f_timer_id), f_paused ? QStringLiteral("1") : QStringLiteral("0"), QString::number(f_msecs)});
}

} // namespace akashi
