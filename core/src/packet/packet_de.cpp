void AOClient::pktRemoveEvidence(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    if (!checkEvidenceAccess(area))
        return;
    bool is_int = false;
    int idx = argv[0].toInt(&is_int);
    if (is_int && idx <= area->evidence().size() && idx >= 0) {
        area->deleteEvidence(idx);
    }
    sendEvidenceList(area);
}