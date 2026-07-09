#pragma once

#include "akashi/service.h"
#include "akashi_core_export.h"
#include "proto/packet_codec.h"
#include "proto/packet_interceptors.h"
#include "proto/packet_registry.h"

namespace akashi {

// Owns the packet pipeline: the handler for each header, the codecs that
// translate each client dialect, and the outbound interceptor chain that
// runs on every packet just before it reaches a client. Registered as
// "akashi.packets".
class AKASHI_CORE_EXPORT PacketService : public IService
{
  public:
    QString serviceId() const override;
    ServiceVersion serviceVersion() const override;

    PacketRegistry &handlers();
    PacketCodecRegistry &codecs();
    PacketInterceptors &outboundInterceptors();

  private:
    PacketRegistry m_handlers;
    PacketCodecRegistry m_codecs;
    PacketInterceptors m_outbound_interceptors;
};

} // namespace akashi
