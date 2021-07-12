void AOClient::pktAddEvidence(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    if (!checkEvidenceAccess(area))
        return;
    AreaData::Evidence evi = {argv[0], argv[1], argv[2]};
    area->appendEvidence(evi);
    sendEvidenceList(area);
}