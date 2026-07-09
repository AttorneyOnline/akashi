#pragma once

#include "akashi_core_export.h"
#include "proto/packet.h"

#include <QList>
#include <QString>

#include <functional>

namespace akashi {

class IPacketContext;

// An ordered, owner-tracked chain of packet rewriters, run on every
// outbound packet just before it reaches a client. Each recipient's chain
// sees its own copy, so interceptors tailor or suppress what individual
// clients receive - the seam for pairing-style extensions and automods.
class AKASHI_CORE_EXPORT PacketInterceptors
{
  public:
    // Keep the packet moving, or end its journey here.
    enum class Verdict
    {
        Pass,
        Drop
    };

    using Interceptor = std::function<Verdict(Packet &f_packet, IPacketContext &f_context)>;

    // Registers an interceptor for one header, or for every packet with an
    // empty header. Lower order runs earlier; equal orders keep their
    // registration order. Returns false without a callable.
    bool registerInterceptor(const QString &f_header, int f_order, Interceptor f_fn,
                             const QString &f_owner_id = {});

    void unregisterAll(const QString &f_owner_id);

    // Runs the chain. Each interceptor matches against the packet's
    // current header, so a rewrite mid-chain changes who sees it next.
    // False means an interceptor dropped the packet.
    bool intercept(Packet &f_packet, IPacketContext &f_context) const;

    int size() const { return m_entries.size(); }

  private:
    struct Entry
    {
        QString header;
        int order;
        Interceptor fn;
        QString owner_id;
    };

    QList<Entry> m_entries;
};

} // namespace akashi
