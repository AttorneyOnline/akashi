#include "core/text_filter_registry.h"

#include "akashi/logging_categories.h"
#include "akashi/thread_assert.h"

#include <QDebug>

#include <algorithm>
#include <utility>

namespace akashi {

TextFilterRegistry::TextFilterRegistry() :
    m_owner_thread(QThread::currentThread())
{}

QString TextFilterRegistry::serviceId() const
{
    return QStringLiteral("akashi.textfilters");
}

ServiceVersion TextFilterRegistry::serviceVersion() const
{
    return {1, 0, 0};
}

void TextFilterRegistry::registerFilter(const QString &f_id, int f_order, TextFilterFn f_filter,
                                        bool f_always_active, const QString &f_owner,
                                        const QSet<TextChannel> &f_channels)
{
    AKASHI_ASSERT_OWNER_THREAD();
    // An id registers once; a duplicate would shadow or double an existing
    // filter while the id stays in the first owner's unregisterAll sweep.
    for (const auto &l_existing : std::as_const(m_entries)) {
        if (l_existing.id == f_id) {
            qCWarning(akashiServer) << "Refused text filter" << f_id << "from" << f_owner
                                    << "- the id is already registered by" << l_existing.owner;
            return;
        }
    }
    Entry l_entry{f_id, f_order, m_counter++, std::move(f_filter), f_always_active, f_owner, f_channels};

    auto l_it = std::lower_bound(m_entries.begin(), m_entries.end(), l_entry,
                                 [](const Entry &a, const Entry &b) {
                                     if (a.order != b.order)
                                         return a.order < b.order;
                                     return a.sequence < b.sequence;
                                 });
    m_entries.insert(l_it, std::move(l_entry));
}

void TextFilterRegistry::unregisterAll(const QString &f_owner)
{
    AKASHI_ASSERT_OWNER_THREAD();
    m_entries.removeIf([&](const Entry &e) { return e.owner == f_owner; });
}

std::optional<QString> TextFilterRegistry::apply(const QString &f_text,
                                                 const QSet<QString> &f_active_ids,
                                                 TextChannel f_channel) const
{
    AKASHI_ASSERT_OWNER_THREAD();
    QString l_text = f_text;
    for (const auto &l_entry : m_entries) {
        if (!l_entry.channels.contains(f_channel))
            continue;
        if (!l_entry.always_active && !f_active_ids.contains(l_entry.id))
            continue;

        auto l_result = l_entry.filter(l_text);
        if (!l_result)
            return std::nullopt;
        l_text = *l_result;
    }
    return l_text;
}

std::optional<QString> TextFilterRegistry::applyFilter(const QString &f_id, const QString &f_text) const
{
    AKASHI_ASSERT_OWNER_THREAD();
    for (const auto &l_entry : m_entries) {
        if (l_entry.id == f_id)
            return l_entry.filter(f_text);
    }
    return f_text;
}

bool TextFilterRegistry::hasFilter(const QString &f_id) const
{
    AKASHI_ASSERT_OWNER_THREAD();
    return std::any_of(m_entries.cbegin(), m_entries.cend(),
                       [&](const Entry &e) { return e.id == f_id; });
}

} // namespace akashi
