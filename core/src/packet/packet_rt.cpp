void AOClient::pktWtCe(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    if (is_wtce_blocked) {
        sendServerMessage("You are blocked from using the judge controls.");
        return;
    }
    if (QDateTime::currentDateTime().toSecsSinceEpoch() - last_wtce_time <= 5)
        return;
    last_wtce_time = QDateTime::currentDateTime().toSecsSinceEpoch();
    server->broadcast(packet, current_area);
    updateJudgeLog(area, this, "WT/CE");
}