void AOClient::pktSelectChar(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    bool argument_ok;
    int selected_char_id = argv[1].toInt(&argument_ok);
    if (!argument_ok) {
        selected_char_id = -1;
        return;
    }

    if (changeCharacter(selected_char_id))
        char_id = selected_char_id;
}