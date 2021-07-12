void AOClient::pktPing(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    // Why does this packet exist
    // At least Crystal made it useful
    // It is now used for ping measurement
    sendPacket("CHECK");
}