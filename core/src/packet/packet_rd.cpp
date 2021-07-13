void AOClient::pktLoadingDone(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    if (hwid == "") {
        // No early connecting!
        socket->close();
        return;
    }

    if (joined) {
        return;
    }

    server->player_count++;
    area->clientJoinedArea();
    joined = true;
    server->updateCharsTaken(area);

    arup(ARUPType::PLAYER_COUNT, true); // Tell everyone there is a new player
    sendEvidenceList(area);

    sendPacket("HP", {"1", QString::number(area->defHP())});
    sendPacket("HP", {"2", QString::number(area->proHP())});
    sendPacket("FA", server->area_names);
    //Here lies OPPASS, the genius of FanatSors who send the modpass to everyone in plain text.
    sendPacket("DONE");
    sendPacket("BN", {area->background()});
  
    sendServerMessage("=== MOTD ===\r\n" + ConfigManager::motd() + "\r\n=============");

    fullArup(); // Give client all the area data
    if (server->timer->isActive()) {
        sendPacket("TI", {"0", "2"});
        sendPacket("TI", {"0", "0", QString::number(QTime(0,0).msecsTo(QTime(0,0).addMSecs(server->timer->remainingTime())))});
    }
    else {
        sendPacket("TI", {"0", "3"});
    }
    for (QTimer* timer : area->timers()) {
        int timer_id = area->timers().indexOf(timer) + 1;
        if (timer->isActive()) {
            sendPacket("TI", {QString::number(timer_id), "2"});
            sendPacket("TI", {QString::number(timer_id), "0", QString::number(QTime(0,0).msecsTo(QTime(0,0).addMSecs(timer->remainingTime())))});
        }
        else {
            sendPacket("TI", {QString::number(timer_id), "3"});
        }
    }
}