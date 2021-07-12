void AOClient::pktRequestChars(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    sendPacket("SC", server->characters);
}