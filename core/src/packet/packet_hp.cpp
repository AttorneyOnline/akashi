void AOClient::pktHpBar(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    if (is_wtce_blocked) {
        sendServerMessage("You are blocked from using the judge controls.");
        return;
    }
    int l_newValue = argv.at(1).toInt();

    if (argv[0] == "1") {
        area->changeHP(AreaData::Side::DEFENCE, l_newValue);
    }
    else if (argv[0] == "2") {
        area->changeHP(AreaData::Side::PROSECUTOR, l_newValue);
    }

    server->broadcast(AOPacket("HP", {"1", QString::number(area->defHP())}), area->index());
    server->broadcast(AOPacket("HP", {"2", QString::number(area->proHP())}), area->index());

    updateJudgeLog(area, this, "updated the penalties");
}