#include "include/packet/aopacket.h"
#include "include/area_data.h"
#include "include/aoclient.h"

class PacketHDID : public AOPacket
{
    PacketHDID(QStringList p_contents);

    /// Implements [hardware ID](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#hard-drive-id).
    virtual void handlePacket(AreaData* area, AOClient& client);
};