#include "proto/packet_service.h"

namespace akashi {

QString PacketService::serviceId() const
{
    return QStringLiteral("akashi.packets");
}

ServiceVersion PacketService::serviceVersion() const
{
    return ServiceVersion{1, 0, 0};
}

PacketRegistry &PacketService::handlers()
{
    return m_handlers;
}

PacketCodecRegistry &PacketService::codecs()
{
    return m_codecs;
}

} // namespace akashi
