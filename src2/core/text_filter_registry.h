#ifndef CORE_TEXT_FILTER_REGISTRY_H
#define CORE_TEXT_FILTER_REGISTRY_H

#include "akashi/service.h"
#include "akashi/text_filter.h"
#include "akashi_core_export.h"

#include <QList>
#include <QSet>
#include <QString>

namespace akashi {

class AKASHI_CORE_EXPORT TextFilterRegistry : public IService
{
  public:
    QString serviceId() const override;
    ServiceVersion serviceVersion() const override;

    void registerFilter(const QString &f_id, int f_order, TextFilterFn f_filter,
                        bool f_always_active, const QString &f_owner = {});

    void unregisterAll(const QString &f_owner);

    std::optional<QString> apply(const QString &f_text, const QSet<QString> &f_active_ids) const;

    bool hasFilter(const QString &f_id) const;

  private:
    struct Entry
    {
        QString id;
        int order;
        int sequence;
        TextFilterFn filter;
        bool always_active;
        QString owner;
    };

    QList<Entry> m_entries;
    int m_counter = 0;
};

} // namespace akashi

#endif // CORE_TEXT_FILTER_REGISTRY_H
