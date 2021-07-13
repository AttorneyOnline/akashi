void AOClient::pktRequestMusic(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    sendPacket("SM", server->area_names + server->music_list);
}
