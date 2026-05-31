#pragma once

#include "akashi/service.h"
#include "akashi_core_export.h"
#include "proto/packet_codec.h"
#include "proto/packet_registry.h"

namespace akashi {

// Owns the packet pipeline: the handler for each header and the codecs
// that translate each client dialect. Registered as "akashi.packets".
class AKASHI_CORE_EXPORT PacketService : public IService
{
  public:
    QString serviceId() const override;
    ServiceVersion serviceVersion() const override;

    PacketRegistry &handlers();
    PacketCodecRegistry &codecs();

  private:
    PacketRegistry m_handlers;
    PacketCodecRegistry m_codecs;
};

} // namespace akashi

