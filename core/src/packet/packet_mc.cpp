void AOClient::pktChangeMusic(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    // Due to historical reasons, this
    // packet has two functions:
    // Change area, and set music.

    // First, we check if the provided
    // argument is a valid song
    QString argument = argv[0];

    for (QString song : server->music_list) {
        if (song == argument || song == "~stop.mp3") { // ~stop.mp3 is a dummy track used by 2.9+
            // We have a song here
            if (is_dj_blocked) {
                sendServerMessage("You are blocked from changing the music.");
                return;
            }
            if (!area->isMusicAllowed() && !checkAuth(ACLFlags.value("CM"))) {
                sendServerMessage("Music is disabled in this area.");
                return;
            }
            QString effects;
            if (argc >= 4)
                effects = argv[3];
            else
                effects = "0";
            QString final_song;
            if (!argument.contains("."))
                final_song = "~stop.mp3";
            else
                final_song = argument;
            AOPacket music_change("MC", {final_song, argv[1], showname, "1", "0", effects});
            area->currentMusic() = final_song;
            area->musicPlayerBy() = showname;
            server->broadcast(music_change, current_area);
            return;
        }
    }

    for (int i = 0; i < server->area_names.length(); i++) {
        QString area = server->area_names[i];
        if(area == argument) {
            changeArea(i);
            break;
        }
    }
}