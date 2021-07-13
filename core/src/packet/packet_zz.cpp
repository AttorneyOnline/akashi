void AOClient::pktModCall(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    for (AOClient* client : server->clients) {
        if (client->authenticated)
            client->sendPacket(packet);
    }
    area->log(current_char, ipid, packet);

    if (ConfigManager::discordWebhookEnabled()) {
        QString name = ooc_name;
        if (ooc_name.isEmpty())
            name = current_char;

        emit server->modcallWebhookRequest(name, server->areas[current_area]->name(), packet.contents[0], area->buffer());
    }
    
    area->flushLogs();
}
