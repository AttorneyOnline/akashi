void AOClient::pktBeginLoad(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    // Evidence isn't loaded during this part anymore
    // As a result, we can always send "0" for evidence length
    // Client only cares about what it gets from LE
    sendPacket("SI", {QString::number(server->characters.length()), "0", QString::number(server->area_names.length() + server->music_list.length())});
}