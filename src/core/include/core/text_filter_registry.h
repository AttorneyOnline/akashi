#pragma once

#include "akashi/service.h"
#include "akashi/text_filter.h"
#include "akashi_core_export.h"

#include <QList>
#include <QSet>
#include <QString>

class QThread;

namespace akashi {

class AKASHI_CORE_EXPORT TextFilterRegistry : public IService
{
  public:
    TextFilterRegistry();

    QString serviceId() const override;
    ServiceVersion serviceVersion() const override;

    // A filter runs only on the channels it registers for; the default
    // keeps every existing filter IC-only. An id registers once - a
    // second registration is refused with a warning and the first owner
    // keeps the filter.
    void registerFilter(const QString &f_id, int f_order, TextFilterFn f_filter,
                        bool f_always_active, const QString &f_owner = {},
                        const QSet<TextChannel> &f_channels = {TextChannel::Ic});

    // The same, for a filter that needs to know who is speaking.
    void registerFilter(const QString &f_id, int f_order, ContextTextFilterFn f_filter,
                        bool f_always_active, const QString &f_owner = {},
                        const QSet<TextChannel> &f_channels = {TextChannel::Ic});

    void unregisterAll(const QString &f_owner);

    std::optional<QString> apply(const QString &f_text, const QSet<QString> &f_active_ids,
                                 TextChannel f_channel = TextChannel::Ic,
                                 const TextFilterContext &f_context = {}) const;

    // Runs one named filter alone, ignoring channels and active sets; the
    // apply_filter rule action reaches single filters through this. An
    // unknown id leaves the text unchanged.
    std::optional<QString> applyFilter(const QString &f_id, const QString &f_text) const;

    bool hasFilter(const QString &f_id) const;

  private:
    struct Entry
    {
        QString id;
        int order;
        int sequence;
        ContextTextFilterFn filter;
        bool always_active;
        QString owner;
        QSet<TextChannel> channels;
    };

    QList<Entry> m_entries;
    int m_counter = 0;
    QThread *m_owner_thread;
};

} // namespace akashi
