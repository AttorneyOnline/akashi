#include "include/packet/aopacket.h"
#include "include/area_data.h"
#include "include/aoclient.h"

class PacketID : public AOPacket
{
    PacketID(QStringList p_contents);

    /**
     * @brief Implements [feature list](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#feature-list) and
     * [player count](https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#player-count).
     */
    virtual void handlePacket(AreaData* area, AOClient& client);
};