void AOClient::pktEditEvidence(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    if (!checkEvidenceAccess(area))
        return;
    bool is_int = false;
    int idx = argv[0].toInt(&is_int);
    AreaData::Evidence evi = {argv[1], argv[2], argv[3]};
    if (is_int && idx <= area->evidence().size() && idx >= 0) {
        area->replaceEvidence(idx, evi);
    }
    sendEvidenceList(area);
}