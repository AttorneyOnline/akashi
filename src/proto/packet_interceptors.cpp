#include "proto/packet_interceptors.h"

#include <algorithm>

namespace akashi {

bool PacketInterceptors::registerInterceptor(const QString &f_header, int f_order, Interceptor f_fn,
                                             const QString &f_owner_id)
{
    if (!f_fn) {
        return false;
    }
    // Insert behind every entry of the same order, so equal orders run in
    // registration order.
    const auto l_at = std::upper_bound(m_entries.cbegin(), m_entries.cend(), f_order,
                                       [](int f_value, const Entry &f_entry) { return f_value < f_entry.order; });
    m_entries.insert(l_at, {f_header, f_order, std::move(f_fn), f_owner_id});
    return true;
}

void PacketInterceptors::unregisterAll(const QString &f_owner_id)
{
    m_entries.removeIf([&f_owner_id](const Entry &f_entry) { return f_entry.owner_id == f_owner_id; });
}

bool PacketInterceptors::intercept(Packet &f_packet, IPacketContext &f_context) const
{
    // Iterate a snapshot so an interceptor may unregister its own owner
    // mid-flight; a removal takes effect from the next packet on.
    const QList<Entry> l_entries = m_entries;
    for (const Entry &l_entry : l_entries) {
        if (!l_entry.header.isEmpty() && l_entry.header != f_packet.header()) {
            continue;
        }
        if (l_entry.fn(f_packet, f_context) == Verdict::Drop) {
            return false;
        }
    }
    return true;
}

} // namespace akashi
