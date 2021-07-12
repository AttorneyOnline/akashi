void AOClient::pktIcChat(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    if (is_muted) {
        sendServerMessage("You cannot speak while muted.");
        return;
    }

    if (!server->can_send_ic_messages) {
        return;
    }

    AOPacket validated_packet = validateIcPacket(packet);
    if (validated_packet.header == "INVALID")
        return;

    if (pos != "")
        validated_packet.contents[5] = pos;

    area->log(current_char, ipid, validated_packet);
    server->broadcast(validated_packet, current_area);
    area->updateLastICMessage(validated_packet.contents);

    server->can_send_ic_messages = false;
    server->next_message_timer.start(ConfigManager::messageFloodguard());
}

AOPacket AOClient::validateIcPacket(AOPacket packet)
{
    // Welcome to the super cursed server-side IC chat validation hell

    // I wanted to use enums or #defines here to make the
    // indicies of the args arrays more readable. But,
    // in typical AO fasion, the indicies for the incoming
    // and outgoing packets are different. Just RTFM.

    AOPacket invalid("INVALID", {});
    QStringList args;
    if (current_char == "" || !joined)
        // Spectators cannot use IC
        return invalid;
    AreaData* area = server->areas[current_area];
    if (area->lockStatus() == AreaData::LockStatus::SPECTATABLE && !area->invited().contains(id) && !checkAuth(ACLFlags.value("BYPASS_LOCKS")))
        // Non-invited players cannot speak in spectatable areas
        return invalid;

    QList<QVariant> incoming_args;
    for (QString arg : packet.contents) {
        incoming_args.append(QVariant(arg));
    }

    // desk modifier
    QStringList allowed_desk_mods;
    allowed_desk_mods << "chat" << "0" << "1" << "2" << "3" << "4" << "5";
    if (allowed_desk_mods.contains(incoming_args[0].toString())) {
        args.append(incoming_args[0].toString());
    }
    else
        return invalid;

    // preanim
    args.append(incoming_args[1].toString());

    // char name
    if (current_char.toLower() != incoming_args[2].toString().toLower()) {
        // Selected char is different from supplied folder name
        // This means the user is INI-swapped
        if (!area->iniswapAllowed()) {
            if (!server->characters.contains(incoming_args[2].toString(), Qt::CaseInsensitive))
                return invalid;
        }
        qDebug() << "INI swap detected from " << getIpid();
    }
    current_iniswap = incoming_args[2].toString();
    args.append(incoming_args[2].toString());

    // emote
    emote = incoming_args[3].toString();
    if (first_person)
        emote = "";
    args.append(emote);

    // message text
    if (incoming_args[4].toString().size() > ConfigManager::maxCharacters())
        return invalid;

    QString incoming_msg = dezalgo(incoming_args[4].toString().trimmed());
    if (!area->lastICMessage().isEmpty()
            && incoming_msg == area->lastICMessage()[4]
            && incoming_msg != "")
        return invalid;

    if (incoming_msg == "" && area->blankpostingAllowed() == false) {
        sendServerMessage("Blankposting has been forbidden in this area.");
        return invalid;
    }

    if (is_gimped) {
        QString gimp_message = ConfigManager::gimpList()[(genRand(1, ConfigManager::gimpList().size() - 1))];
        incoming_msg = gimp_message;
    }

    if (is_shaken) {
        QStringList parts = incoming_msg.split(" ");
        std::random_shuffle(parts.begin(), parts.end());
        incoming_msg = parts.join(" ");
    }

    if (is_disemvoweled) {
        QString disemvoweled_message = incoming_msg.remove(QRegExp("[AEIOUaeiou]"));
        incoming_msg = disemvoweled_message;
    }

    last_message = incoming_msg;
    args.append(incoming_msg);

    // side
    // this is validated clientside so w/e
    args.append(incoming_args[5].toString());
    if (pos != incoming_args[5].toString()) {
        pos = incoming_args[5].toString();
        updateEvidenceList(server->areas[current_area]);
    }

    // sfx name
    args.append(incoming_args[6].toString());

    // emote modifier
    // Now, gather round, y'all. Here is a story that is truly a microcosm of the AO dev experience.
    // If this value is a 4, it will crash the client. Why? Who knows, but it does.
    // Now here is the kicker: in certain versions, the client would incorrectly send a 4 here
    // For a long time, by configuring the client to do a zoom with a preanim, it would send 4
    // This would crash everyone else's client, and the feature had to be disabled
    // But, for some reason, nobody traced the cause of this issue for many many years.
    // The serverside fix is needed to ensure invalid values are not sent, because the client sucks
    int emote_mod = incoming_args[7].toInt();

    if (emote_mod == 4)
        emote_mod = 6;
    if (emote_mod != 0 && emote_mod != 1 && emote_mod != 2 && emote_mod != 5 && emote_mod != 6)
        return invalid;
    args.append(QString::number(emote_mod));

    // char id
    if (incoming_args[8].toInt() != char_id)
        return invalid;
    args.append(incoming_args[8].toString());

    // sfx delay
    args.append(incoming_args[9].toString());

    // objection modifier
    if (incoming_args[10].toString().contains("4")) {
        // custom shout includes text metadata
        args.append(incoming_args[10].toString());
    }
    else {
        int obj_mod = incoming_args[10].toInt();
        if (obj_mod != 0 && obj_mod != 1 && obj_mod != 2 && obj_mod != 3)
            return invalid;
        args.append(QString::number(obj_mod));
    }

    // evidence
    int evi_idx = incoming_args[11].toInt();
    if (evi_idx > area->evidence().length())
        return invalid;
    args.append(QString::number(evi_idx));

    // flipping
    int flip = incoming_args[12].toInt();
    if (flip != 0 && flip != 1)
        return invalid;
    flipping = QString::number(flip);
    args.append(flipping);

    // realization
    int realization = incoming_args[13].toInt();
    if (realization != 0 && realization != 1)
        return invalid;
    args.append(QString::number(realization));

    // text color
    int text_color = incoming_args[14].toInt();
    if (text_color < 0 || text_color > 11)
        return invalid;
    args.append(QString::number(text_color));

    // 2.6 packet extensions
    if (incoming_args.length() > 15) {
        // showname
        QString incoming_showname = dezalgo(incoming_args[15].toString().trimmed());
        if (!(incoming_showname == current_char || incoming_showname.isEmpty()) && !area->shownameAllowed()) {
            sendServerMessage("Shownames are not allowed in this area!");
            return invalid;
        }
        if (incoming_showname.length() > 30) {
            sendServerMessage("Your showname is too long! Please limit it to under 30 characters");
            return invalid;
        }

        // if the raw input is not empty but the trimmed input is, use a single space
        if (incoming_showname.isEmpty() && !incoming_args[15].toString().isEmpty())
            incoming_showname = " ";
        args.append(incoming_showname);
        showname = incoming_showname;

        // other char id
        // things get a bit hairy here
        // don't ask me how this works, because i don't know either
        QStringList pair_data = incoming_args[16].toString().split("^");
        pairing_with = pair_data[0].toInt();
        QString front_back = "";
        if (pair_data.length() > 1)
            front_back = "^" + pair_data[1];
        int other_charid = pairing_with;
        bool pairing = false;
        QString other_name = "0";
        QString other_emote = "0";
        QString other_offset = "0";
        QString other_flip = "0";
        for (AOClient* client : server->clients) {
            if (client->pairing_with == char_id
                    && other_charid != char_id
                    && client->char_id == pairing_with
                    && client->pos == pos) {
                other_name = client->current_iniswap;
                other_emote = client->emote;
                other_offset = client->offset;
                other_flip = client->flipping;
                pairing = true;
            }
        }
        if (!pairing) {
            other_charid = -1;
            front_back = "";
        }
        args.append(QString::number(other_charid) + front_back);
        args.append(other_name);
        args.append(other_emote);

        // self offset
        offset = incoming_args[17].toString();
        // versions 2.6-2.8 cannot validate y-offset so we send them just the x-offset
        if ((version.release == 2) && (version.major == 6 || version.major == 7 || version.major == 8)) {
            QString x_offset = offset.split("&")[0];
            args.append(x_offset);
            QString other_x_offset = other_offset.split("&")[0];
            args.append(other_x_offset);
        }
        else {
            args.append(offset);
            args.append(other_offset);
        }
        args.append(other_flip);

        // immediate text processing
        int immediate = incoming_args[18].toInt();
        if (area->forceImmediate()) {
            if (args[7] == "1" || args[7] == "2") {
                args[7] = "0";
                immediate = 1;
            }
            else if (args[7] == "6") {
                args[7] = "5";
                immediate = 1;
            }
        }
        if (immediate != 1 && immediate != 0)
            return invalid;
        args.append(QString::number(immediate));
    }

    // 2.8 packet extensions
    if (incoming_args.length() > 19) {
        // sfx looping
        int sfx_loop = incoming_args[19].toInt();
        if (sfx_loop != 0 && sfx_loop != 1)
            return invalid;
        args.append(QString::number(sfx_loop));

        // screenshake
        int screenshake = incoming_args[20].toInt();
        if (screenshake != 0 && screenshake != 1)
            return invalid;
        args.append(QString::number(screenshake));

        // frames shake
        args.append(incoming_args[21].toString());

        // frames realization
        args.append(incoming_args[22].toString());

        // frames sfx
        args.append(incoming_args[23].toString());

        // additive
        int additive = incoming_args[24].toInt();
        if (additive != 0 && additive != 1)
            return invalid;
        else if (area->lastICMessage().isEmpty()){
            additive = 0;
        }
        else if (!(char_id == area->lastICMessage()[8].toInt())) {
            additive = 0;
        }
        else if (additive == 1) {
            args[4].insert(0, " ");
        }
        args.append(QString::number(additive));

        // effect
        args.append(incoming_args[25].toString());
    }

    //Testimony playback
    if (area->testimonyRecording() == AreaData::TestimonyRecording::RECORDING || area->testimonyRecording() == AreaData::TestimonyRecording::ADD) {
        if (args[5] != "wit")
            return AOPacket("MS", args);

        if (area->statement() == -1) {
            args[4] = "~~\\n-- " + args[4] + " --";
            args[14] = "3";
            server->broadcast(AOPacket("RT",{"testimony1"}), current_area);
        }
        addStatement(args);
    }
    else if (area->testimonyRecording() == AreaData::TestimonyRecording::UPDATE) {
        args = updateStatement(args);
    }
    else if (area->testimonyRecording() == AreaData::TestimonyRecording::PLAYBACK) {
        AreaData::TestimonyProgress l_progress;

        if (args[4] == ">") {
            pos = "wit";
            auto l_statement = area->jumpToStatement(area->statement() +1);
            args = l_statement.first;
            l_progress = l_statement.second;

            if (l_progress == AreaData::TestimonyProgress::LOOPED) {
                sendServerMessageArea("Last statement reached. Looping to first statement.");
            }
        }
        if (args[4] == "<") {
            pos = "wit";
            auto l_statement = area->jumpToStatement(area->statement() - 1);
            args = l_statement.first;
            l_progress = l_statement.second;

            if (l_progress == AreaData::TestimonyProgress::STAYED_AT_FIRST) {
                sendServerMessage("First statement reached.");
            }
        }

        QString decoded_message = decodeMessage(args[4]); //Get rid of that pesky encoding first.
        QRegularExpression jump("(?<arrow>>)(?<int>[0,1,2,3,4,5,6,7,8,9]+)");
        QRegularExpressionMatch match = jump.match(decoded_message);
        if (match.hasMatch()) {
            pos = "wit";
            auto l_statement = area->jumpToStatement(match.captured("int").toInt());
            args = l_statement.first;
            l_progress = l_statement.second;


            switch (l_progress){
            case AreaData::TestimonyProgress::LOOPED:
            {
                sendServerMessageArea("Last statement reached. Looping to first statement.");
            }
            case AreaData::TestimonyProgress::STAYED_AT_FIRST:
            {
                sendServerMessage("First statement reached.");
            }
            case AreaData::TestimonyProgress::OK:
            default:
                // No need to handle.
                break;
            }
        }
    }

    return AOPacket("MS", args);
}