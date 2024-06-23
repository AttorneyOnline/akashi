#ifndef PACKET_INFO_H
#define PACKET_INFO_H

#include "acl_roles_handler.h"

/// Describes a packet's interpretation details.
class PacketInfo
{
  public:
    ACLRole::Permission acl_permission; //!< The permissions necessary for the packet.
    int min_args;                       //!< The minimum arguments needed for the packet to be interpreted correctly / make sense.
    QString header;
};
#endif
