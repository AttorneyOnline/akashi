#pragma once

#include "akashi_core_export.h"
#include "proto/message.h"
#include "proto/packet_context.h"

#include <QString>

namespace akashi {

// What the dispatcher checks before a handler runs.
// The permission is an ACL caption like "KICK"; empty means anyone.
struct PacketSpec
{
    QString header;
    int min_args = 0;
    QString required_permission;
};

// Handles one packet type. Works on the decoded message through the
// narrow context, with no knowledge of the client's dialect.
class AKASHI_CORE_EXPORT PacketHandler
{
  public:
    virtual ~PacketHandler() = default;
    virtual void handle(const Message &f_message, IPacketContext &f_context) const = 0;
};

} // namespace akashi
