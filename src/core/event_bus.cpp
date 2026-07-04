#include "core/event_bus.h"

#include "akashi/thread_assert.h"

#include <algorithm>

namespace akashi {

EventBus::EventBus() :
    m_owner_thread(QThread::currentThread())
{}

QString EventBus::serviceId() const
{
    return QStringLiteral("akashi.events");
}

ServiceVersion EventBus::serviceVersion() const
{
    return {1, 0, 0};
}

int EventBus::subscribeRaw(int f_type_id, EventPhase f_phase, int f_priority,
                           std::function<void(void *)> f_handler, const QString &f_owner)
{
    AKASHI_ASSERT_OWNER_THREAD();
    const int l_handle = m_next_handle++;

    auto l_it = std::lower_bound(m_entries.begin(), m_entries.end(), f_priority,
                                 [](const Entry &e, int p) { return e.priority < p; });

    m_entries.insert(l_it, {l_handle, f_type_id, f_phase, f_priority, std::move(f_handler), f_owner});
    return l_handle;
}

bool EventBus::gateRaw(int f_type_id, void *f_event)
{
    AKASHI_ASSERT_OWNER_THREAD();
    auto *l_base = static_cast<Event *>(f_event);
    for (const Entry &l_entry : std::as_const(m_entries)) {
        if (l_entry.type_id == f_type_id && l_entry.phase == EventPhase::Before) {
            l_entry.handler(f_event);
            if (l_base->cancelled)
                return false;
        }
    }
    return true;
}

void EventBus::notifyRaw(int f_type_id, void *f_event)
{
    AKASHI_ASSERT_OWNER_THREAD();
    for (const Entry &l_entry : std::as_const(m_entries)) {
        if (l_entry.type_id == f_type_id && l_entry.phase == EventPhase::After) {
            l_entry.handler(f_event);
        }
    }
}

void EventBus::unsubscribe(int f_handle)
{
    AKASHI_ASSERT_OWNER_THREAD();
    m_entries.removeIf([f_handle](const Entry &e) { return e.handle == f_handle; });

    for (auto &l_list : m_custom) {
        l_list.removeIf([f_handle](const CustomEntry &e) { return e.handle == f_handle; });
    }
}

void EventBus::unsubscribeAll(const QString &f_owner)
{
    AKASHI_ASSERT_OWNER_THREAD();
    m_entries.removeIf([&f_owner](const Entry &e) { return e.owner == f_owner; });

    for (auto it = m_custom.begin(); it != m_custom.end();) {
        it->removeIf([&f_owner](const CustomEntry &e) { return e.owner == f_owner; });
        if (it->isEmpty())
            it = m_custom.erase(it);
        else
            ++it;
    }
}

int EventBus::subscribeCustom(const QString &f_name, EventPhase f_phase,
                              std::function<void(const QVariantMap &)> f_handler,
                              const QString &f_owner)
{
    AKASHI_ASSERT_OWNER_THREAD();
    const int l_handle = m_next_handle++;
    m_custom[f_name].append({l_handle, f_phase, std::move(f_handler), f_owner});
    return l_handle;
}

void EventBus::publishCustom(const QString &f_name, const QVariantMap &f_payload)
{
    AKASHI_ASSERT_OWNER_THREAD();
    auto it = m_custom.constFind(f_name);
    if (it == m_custom.constEnd())
        return;

    for (const CustomEntry &l_entry : *it) {
        l_entry.handler(f_payload);
    }
}

} // namespace akashi
