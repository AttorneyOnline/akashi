#include "proto/packet_registry.h"

#include "core/thread_assert.h"

namespace akashi {

PacketRegistry::PacketRegistry() :
    m_owner_thread(QThread::currentThread())
{}

bool PacketRegistry::registerHandler(const PacketSpec &f_spec, std::shared_ptr<PacketHandler> f_handler,
                                     const QString &f_owner_id)
{
    AKASHI_ASSERT_OWNER_THREAD();
    if (f_spec.header.isEmpty() || !f_handler || m_entries.contains(f_spec.header)) {
        return false;
    }
    m_entries.insert(f_spec.header, Entry{f_spec, f_handler, f_owner_id});
    return true;
}

void PacketRegistry::unregisterAll(const QString &f_owner_id)
{
    AKASHI_ASSERT_OWNER_THREAD();
    for (auto l_iterator = m_entries.begin(); l_iterator != m_entries.end();) {
        if (l_iterator.value().owner == f_owner_id) {
            l_iterator = m_entries.erase(l_iterator);
        }
        else {
            ++l_iterator;
        }
    }
}

std::optional<PacketSpec> PacketRegistry::spec(const QString &f_header) const
{
    AKASHI_ASSERT_OWNER_THREAD();
    const auto l_iterator = m_entries.constFind(f_header);
    if (l_iterator == m_entries.constEnd()) {
        return std::nullopt;
    }
    return l_iterator.value().spec;
}

std::shared_ptr<PacketHandler> PacketRegistry::handler(const QString &f_header) const
{
    AKASHI_ASSERT_OWNER_THREAD();
    const auto l_iterator = m_entries.constFind(f_header);
    if (l_iterator == m_entries.constEnd()) {
        return nullptr;
    }
    return l_iterator.value().handler;
}

} // namespace akashi
