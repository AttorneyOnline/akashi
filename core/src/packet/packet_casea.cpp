void AOClient::pktAnnounceCase(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    QString case_title = argv[0];
    QStringList needed_roles;
    QList<bool> needs_list;
    for (int i = 1; i <=5; i++) {
        bool is_int = false;
        bool need = argv[i].toInt(&is_int);
        if (!is_int)
            return;
        needs_list.append(need);
    }
    QStringList roles = {"defense attorney", "prosecutor", "judge", "jurors", "stenographer"};
    for (int i = 0; i < 5; i++) {
      if (needs_list[i])
        needed_roles.append(roles[i]);
    }
    if (needed_roles.isEmpty())
        return;

    QString message = "=== Case Announcement ===\r\n" + (ooc_name == "" ? current_char : ooc_name) + " needs " + needed_roles.join(", ") + " for " + (case_title == "" ? "a case" : case_title) + "!";

    QList<AOClient*> clients_to_alert;
    // here lies morton, RIP
    QSet<bool> needs_set = needs_list.toSet();
    for (AOClient* client : server->clients) {
        QSet<bool> matches = client->casing_preferences.toSet().intersect(needs_set);
        if (!matches.isEmpty() && !clients_to_alert.contains(client))
            clients_to_alert.append(client);
    }

    for (AOClient* client : clients_to_alert) {
        client->sendPacket(AOPacket("CASEA", {message, argv[1], argv[2], argv[3], argv[4], argv[5], "1"}));
        // you may be thinking, "hey wait a minute the network protocol documentation doesn't mention that last argument!"
        // if you are in fact thinking that, you are correct! it is not in the documentation!
        // however for some inscrutable reason Attorney Online 2 will outright reject a CASEA packet that does not have
        // at least 7 arguments despite only using the first 6. Cera, i kneel. you have truly broken me.
    }
}