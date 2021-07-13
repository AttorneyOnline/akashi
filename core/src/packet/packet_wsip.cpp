void AOClient::pktWebSocketIp(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    // Special packet to set remote IP from the webao proxy
    // Only valid if from a local ip
    if (remote_ip.isLoopback()) {
#ifdef NET_DEBUG
        qDebug() << "ws ip set to" << argv[0];
#endif
        remote_ip = QHostAddress(argv[0]);
        calculateIpid();
        auto ban = server->db_manager->isIPBanned(ipid);
        if (ban.first) {
            sendPacket("BD", {ban.second});
            socket->close();
            return;
        }

        int multiclient_count = 0;
        for (AOClient* joined_client : server->clients) {
            if (remote_ip.isEqual(joined_client->remote_ip))
                multiclient_count++;
        }

        if (multiclient_count > ConfigManager::multiClientLimit()) {
            socket->close();
            return;
        }
    }
}