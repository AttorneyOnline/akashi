#pragma once

#include "akashi/event.h"
#include "akashi/service.h"
#include "akashi_core_export.h"

#include <QHash>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QVariantMap>

#include <functional>

class QThread;

namespace akashi {

class AKASHI_CORE_EXPORT EventBus : public IService
{
  public:
    EventBus();

    QString serviceId() const override;
    ServiceVersion serviceVersion() const override;

    int subscribeRaw(int f_type_id, EventPhase f_phase, int f_priority,
                     std::function<void(void *)> f_handler, const QString &f_owner = {});
    void unsubscribe(int f_handle);
    void unsubscribeAll(const QString &f_owner);

    bool gateRaw(int f_type_id, void *f_event);
    void notifyRaw(int f_type_id, void *f_event);

    int subscribeCustom(const QString &f_name, EventPhase f_phase,
                        std::function<void(const QVariantMap &)> f_handler,
                        const QString &f_owner = {});
    void publishCustom(const QString &f_name, const QVariantMap &f_payload);

    template <typename E>
    int subscribe(EventPhase f_phase, int f_priority, std::function<void(E &)> f_handler,
                  const QString &f_owner = {})
    {
        return subscribeRaw(
            qMetaTypeId<E>(), f_phase, f_priority, [h = std::move(f_handler)](void *raw) { h(*static_cast<E *>(raw)); }, f_owner);
    }

    template <typename E>
    bool gate(E &f_event)
    {
        return gateRaw(qMetaTypeId<E>(), &f_event);
    }

    template <typename E>
    void notify(E &f_event)
    {
        notifyRaw(qMetaTypeId<E>(), &f_event);
    }

  private:
    struct Entry
    {
        int handle;
        int type_id;
        EventPhase phase;
        int priority;
        std::function<void(void *)> handler;
        QString owner;
    };

    struct CustomEntry
    {
        int handle;
        EventPhase phase;
        std::function<void(const QVariantMap &)> handler;
        QString owner;
    };

    int m_next_handle = 1;
    QList<Entry> m_entries;
    QHash<QString, QList<CustomEntry>> m_custom;
    QThread *m_owner_thread;
};

} // namespace akashi
