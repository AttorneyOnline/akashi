void AOClient::pktSetCase(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    QList<bool> prefs_list;
    for (int i = 2; i <=6; i++) {
        bool is_int = false;
        bool pref = argv[i].toInt(&is_int);
        if (!is_int)
            return;
        prefs_list.append(pref);
    }
    casing_preferences = prefs_list;
}