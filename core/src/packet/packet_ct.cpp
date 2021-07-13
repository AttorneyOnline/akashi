void AOClient::pktOocChat(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
    if (is_ooc_muted) {
        sendServerMessage("You are OOC muted, and cannot speak.");
        return;
    }

    ooc_name = dezalgo(argv[0]).replace(QRegExp("\\[|\\]|\\{|\\}|\\#|\\$|\\%|\\&"), ""); // no fucky wucky shit here
    if (ooc_name.isEmpty() || ooc_name == ConfigManager::serverName()) // impersonation & empty name protection
        return;

    if (ooc_name.length() > 30) {
        sendServerMessage("Your name is too long! Please limit it to under 30 characters.");
        return;
    }

    if (is_logging_in) {
        loginAttempt(argv[1]);
        return;
    }
    
    QString message = dezalgo(argv[1]);
    if (message.length() == 0 || message.length() > ConfigManager::maxCharacters())
        return;
    AOPacket final_packet("CT", {ooc_name, message, "0"});
    if(message.at(0) == '/') {
        QStringList cmd_argv = message.split(" ", QString::SplitBehavior::SkipEmptyParts);
        QString command = cmd_argv[0].trimmed().toLower();
        command = command.right(command.length() - 1);
        cmd_argv.removeFirst();
        int cmd_argc = cmd_argv.length();

        handleCommand(command, cmd_argc, cmd_argv);
        area->logCmd(current_char, ipid, command, cmd_argv);
        return;
    }
    else {
        server->broadcast(final_packet, current_area);
    }
    area->log(current_char, ipid, final_packet);
}