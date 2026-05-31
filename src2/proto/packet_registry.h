#pragma once

#include "akashi_core_export.h"
#include "proto/packet_handler.h"

#include <QHash>
#include <QString>

#include <memory>
#include <optional>

class QThread;

namespace akashi {

// Holds the handler for each packet header. One instance per server,
// with owner tracking so a plugin's handlers can be removed together.
class AKASHI_CORE_EXPORT PacketRegistry
{
  public:
    PacketRegistry();

    // Registers a handler for the spec's header. Fails if the header is taken.
    bool registerHandler(const PacketSpec &f_spec, std::shared_ptr<PacketHandler> f_handler,
                         const QString &f_owner_id = QString());

    // Removes every handler registered by the given owner, for plugin unloading.
    void unregisterAll(const QString &f_owner_id);

    std::optional<PacketSpec> spec(const QString &f_header) const;
    std::shared_ptr<PacketHandler> handler(const QString &f_header) const;

  private:
    struct Entry
    {
        PacketSpec spec;
        std::shared_ptr<PacketHandler> handler;
        QString owner;
    };

    QHash<QString, Entry> m_entries;
    QThread *m_owner_thread;
};

} // namespace akashi

