//////////////////////////////////////////////////////////////////////////////////////
//    akashi - a server for Attorney Online 2                                       //
//    Copyright (C) 2020  scatterflower                                           //
//                                                                                  //
//    This program is free software: you can redistribute it and/or modify          //
//    it under the terms of the GNU Affero General Public License as                //
//    published by the Free Software Foundation, either version 3 of the            //
//    License, or (at your option) any later version.                               //
//                                                                                  //
//    This program is distributed in the hope that it will be useful,               //
//    but WITHOUT ANY WARRANTY{} without even the implied warranty of                //
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                 //
//    GNU Affero General Public License for more details.                           //
//                                                                                  //
//    You should have received a copy of the GNU Affero General Public License      //
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.        //
//////////////////////////////////////////////////////////////////////////////////////
#include "include/aoclient.h"

#include <QQueue>

#include "include/akashidefs.h"
#include "include/area_data.h"
#include "include/config_manager.h"
#include "include/db_manager.h"
#include "include/music_manager.h"
#include "include/packet/packet_factory.h"
#include "include/server.h"

/*
void AOClient::pktDefault(AreaData *area, int argc, QStringList argv, AOPacket packet)
{
    Q_UNUSED(area);
    Q_UNUSED(argc);
    Q_UNUSED(argv);
#ifdef NET_DEBUG
    qDebug() << "Unimplemented packet:" << packet.getHeader() << packet.getContent();
#else
    Q_UNUSED(packet);
#endif
}

void AOClient::pktHardwareId(AreaData *area, int argc, QStringList argv, AOPacket packet)
{
    Q_UNUSED(area);
    Q_UNUSED(argc);
    Q_UNUSED(packet);

    QString l_incoming_hwid = argv[0];
    if (l_incoming_hwid.isEmpty() || !m_hwid.isEmpty()) {
        // No double sending or empty HWIDs!
        sendPacket(AOPacket("BD", {"A protocol error has been encountered. Packet : HI"}));
        m_socket->close();
        return;
    }

    m_hwid = l_incoming_hwid;
    emit server->logConnectionAttempt(m_remote_ip.toString(), m_ipid, m_hwid);
    auto l_ban = server->getDatabaseManager()->isHDIDBanned(m_hwid);
    if (l_ban.first) {
        sendPacket("BD", {l_ban.second + "\nBan ID: " + QString::number(server->getDatabaseManager()->getBanID(m_hwid))});
        m_socket->close();
        return;
    }
    sendPacket("ID", {QString::number(m_id), "akashi", QCoreApplication::applicationVersion()});
}

void AOClient::pktSoftwareId(AreaData *area, int argc, QStringList argv, AOPacket packet)
{
    Q_UNUSED(area);
    Q_UNUSED(argc);
    Q_UNUSED(packet);

    if (m_version.major == 2) {
        // No double sending of the ID packet!
        sendPacket(AOPacket("BD", {"A protocol error has been encountered. Packet : ID"}));
        m_socket->close();
        return;
    }

    // Full feature list as of AO 2.8.5
    // The only ones that are critical to ensuring the server works are
    // "noencryption" and "fastloading"
    QStringList l_feature_list = {
        "noencryption", "yellowtext", "prezoom",
        "flipping", "customobjections", "fastloading",
        "deskmod", "evidence", "cccc_ic_support",
        "arup", "casing_alerts", "modcall_reason",
        "looping_sfx", "additive", "effects",
        "y_offset", "expanded_desk_mods", "auth_packet"};

    m_version.string = argv[1];
    QRegularExpression rx("\\b(\\d+)\\.(\\d+)\\.(\\d+)\\b"); // matches X.X.X (e.g. 2.9.0, 2.4.10, etc.)
    QRegularExpressionMatch l_match = rx.match(m_version.string);
    if (l_match.hasMatch()) {
        m_version.release = l_match.captured(1).toInt();
        m_version.major = l_match.captured(2).toInt();
        m_version.minor = l_match.captured(3).toInt();
    }
    if (argv[0] == "webAO") {
        m_version.release = 2;
        m_version.major = 10;
        m_version.minor = 0;
    }

    if (m_version.release != 2) {
        // No valid ID packet resolution.
        sendPacket(AOPacket("BD", {"A protocol error has been encountered. Packet : ID\nMajor version not recognised."}));
        m_socket->close();
        return;
    }

    sendPacket("PN", {QString::number(server->getPlayerCount()), QString::number(ConfigManager::maxPlayers()), ConfigManager::serverDescription()});
    sendPacket("FL", l_feature_list);

    if (ConfigManager::assetUrl().isValid()) {
        QByteArray l_asset_url = ConfigManager::assetUrl().toEncoded(QUrl::EncodeSpaces);
        sendPacket("ASS", {l_asset_url});
    }
}

void AOClient::pktBeginLoad(AreaData *area, int argc, QStringList argv, AOPacket packet)
{
    Q_UNUSED(area);
    Q_UNUSED(argc);
    Q_UNUSED(argv);
    Q_UNUSED(packet);

    // Evidence isn't loaded during this part anymore
    // As a result, we can always send "0" for evidence length
    // Client only cares about what it gets from LE
    sendPacket("SI", {QString::number(server->getCharacterCount()), "0", QString::number(server->getAreaCount() + server->getMusicList().length())});
}

void AOClient::pktRequestChars(AreaData *area, int argc, QStringList argv, AOPacket packet)
{
    Q_UNUSED(area);
    Q_UNUSED(argc);
    Q_UNUSED(argv);
    Q_UNUSED(packet);

    sendPacket("SC", server->getCharacters());
}

void AOClient::pktRequestMusic(AreaData *area, int argc, QStringList argv, AOPacket packet)
{
    Q_UNUSED(area);
    Q_UNUSED(argc);
    Q_UNUSED(argv);
    Q_UNUSED(packet);

    sendPacket("SM", server->getAreaNames() + server->getMusicList());
}

void AOClient::pktLoadingDone(AreaData *area, int argc, QStringList argv, AOPacket packet)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);
    Q_UNUSED(packet);

    if (m_hwid == "") {
        // No early connecting!
        m_socket->close();
        return;
    }

    if (m_joined) {
        return;
    }

    m_joined = true;
    server->updateCharsTaken(area);
    sendEvidenceList(area);
    sendPacket("HP", {"1", QString::number(area->defHP())});
    sendPacket("HP", {"2", QString::number(area->proHP())});
    sendPacket("FA", server->getAreaNames());
    // Here lies OPPASS, the genius of FanatSors who send the modpass to everyone in plain text.
    sendPacket("DONE");
    sendPacket("BN", {area->background()});

    sendServerMessage("=== MOTD ===\r\n" + ConfigManager::motd() + "\r\n=============");

    fullArup(); // Give client all the area data
    if (server->timer->isActive()) {
        sendPacket("TI", {"0", "2"});
        sendPacket("TI", {"0", "0", QString::number(QTime(0, 0).msecsTo(QTime(0, 0).addMSecs(server->timer->remainingTime())))});
    }
    else {
        sendPacket("TI", {"0", "3"});
    }
    const QList<QTimer *> l_timers = area->timers();
    for (QTimer *l_timer : l_timers) {
        int l_timer_id = area->timers().indexOf(l_timer) + 1;
        if (l_timer->isActive()) {
            sendPacket("TI", {QString::number(l_timer_id), "2"});
            sendPacket("TI", {QString::number(l_timer_id), "0", QString::number(QTime(0, 0).msecsTo(QTime(0, 0).addMSecs(l_timer->remainingTime())))});
        }
        else {
            sendPacket("TI", {QString::number(l_timer_id), "3"});
        }
    }
    emit joined();
    area->clientJoinedArea(-1, m_id);
    arup(ARUPType::PLAYER_COUNT, true); // Tell everyone there is a new player
}

void AOClient::pktCharPassword(AreaData *area, int argc, QStringList argv, AOPacket packet)
{
    Q_UNUSED(area);
    Q_UNUSED(argc);
    Q_UNUSED(packet);

    m_password = argv[0];
}

void AOClient::pktSelectChar(AreaData *area, int argc, QStringList argv, AOPacket packet)
{
    Q_UNUSED(area);
    Q_UNUSED(argc);
    Q_UNUSED(packet);

    bool argument_ok;
    int l_selected_char_id = argv[1].toInt(&argument_ok);
    if (!argument_ok) {
        l_selected_char_id = SPECTATOR_ID;
    }

    if (l_selected_char_id < -1 || l_selected_char_id > server->getCharacters().size() - 1) {
        sendPacket(AOPacket("KK", {"A protocol error has been encountered.Packet : CC\nCharacter ID out of range."}));
        m_socket->close();
    }

    if (changeCharacter(l_selected_char_id))
        m_char_id = l_selected_char_id;

    if (m_char_id > SPECTATOR_ID) {
        setSpectator(false);
    }
}

void AOClient::pktIcChat(AreaData *area, int argc, QStringList argv, AOPacket packet)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    if (m_is_muted) {
        sendServerMessage("You cannot speak while muted.");
        return;
    }

    if (!area->isMessageAllowed() || !server->isMessageAllowed()) {
        return;
    }

    AOPacket validated_packet = validateIcPacket(packet);
    if (validated_packet.getHeader() == "INVALID")
        return;

    if (m_pos != "")
        validated_packet.setContentField(5, m_pos);

    server->broadcast(validated_packet, m_current_area);
    emit logIC((m_current_char + " " + m_showname), m_ooc_name, m_ipid, server->getAreaById(m_current_area)->name(), m_last_message);
    area->updateLastICMessage(validated_packet.getContent());

    area->startMessageFloodguard(ConfigManager::messageFloodguard());
    server->startMessageFloodguard(ConfigManager::globalMessageFloodguard());
}

void AOClient::pktOocChat(AreaData *area, int argc, QStringList argv, AOPacket packet)
{
    Q_UNUSED(argc);
    Q_UNUSED(packet);

    if (m_is_ooc_muted) {
        sendServerMessage("You are OOC muted, and cannot speak.");
        return;
    }

    m_ooc_name = dezalgo(argv[0]).replace(QRegExp("\\[|\\]|\\{|\\}|\\#|\\$|\\%|\\&"), ""); // no fucky wucky shit here
    if (m_ooc_name.isEmpty() || m_ooc_name == ConfigManager::serverName())                 // impersonation & empty name protection
        return;

    if (m_ooc_name.length() > 30) {
        sendServerMessage("Your name is too long! Please limit it to under 30 characters.");
        return;
    }

    if (m_is_logging_in) {
        loginAttempt(argv[1]);
        return;
    }

    QString l_message = dezalgo(argv[1]);
    if (l_message.length() == 0 || l_message.length() > ConfigManager::maxCharacters())
        return;
    AOPacket final_packet("CT", {m_ooc_name, l_message, "0"});
    if (l_message.at(0) == '/') {
        QStringList l_cmd_argv = l_message.split(" ", akashi::SkipEmptyParts);
        QString l_command = l_cmd_argv[0].trimmed().toLower();
        l_command = l_command.right(l_command.length() - 1);
        l_cmd_argv.removeFirst();
        int l_cmd_argc = l_cmd_argv.length();

        handleCommand(l_command, l_cmd_argc, l_cmd_argv);
        emit logCMD((m_current_char + " " + m_showname), m_ipid, m_ooc_name, l_command, l_cmd_argv, server->getAreaById(m_current_area)->name());
        return;
    }
    else {
        server->broadcast(final_packet, m_current_area);
    }
    emit logOOC((m_current_char + " " + m_showname), m_ooc_name, m_ipid, area->name(), l_message);
}

void AOClient::pktPing(AreaData *area, int argc, QStringList argv, AOPacket packet)
{
    Q_UNUSED(area);
    Q_UNUSED(argc);
    Q_UNUSED(argv);
    Q_UNUSED(packet);

    // Why does this packet exist
    // At least Crystal made it useful
    // It is now used for ping measurement
    sendPacket("CHECK");
}

void AOClient::pktChangeMusic(AreaData *area, int argc, QStringList argv, AOPacket packet)
{
    Q_UNUSED(packet);

    // Due to historical reasons, this
    // packet has two functions:
    // Change area, and set music.

    // First, we check if the provided
    // argument is a valid song
    QString l_argument = argv[0];

    if (server->getMusicList().contains(l_argument) || m_music_manager->isCustom(m_current_area, l_argument) || l_argument == "~stop.mp3") { // ~stop.mp3 is a dummy track used by 2.9+
        // We have a song here

        if (m_is_spectator) {
            sendServerMessage("Spectator are blocked from changing the music.");
            return;
        }

        if (m_is_dj_blocked) {
            sendServerMessage("You are blocked from changing the music.");
            return;
        }
        if (!area->isMusicAllowed() && !checkPermission(ACLRole::CM)) {
            sendServerMessage("Music is disabled in this area.");
            return;
        }
        QString l_effects;
        if (argc >= 4)
            l_effects = argv[3];
        else
            l_effects = "0";
        QString l_final_song;

        // As categories can be used to stop music we need to check if it has a dot for the extension. If not, we assume its a category.
        if (!l_argument.contains("."))
            l_final_song = "~stop.mp3";
        else
            l_final_song = l_argument;

        // Jukebox intercepts the direct playing of messages.
        if (area->isjukeboxEnabled()) {
            QString l_jukebox_reply = area->addJukeboxSong(l_final_song);
            sendServerMessage(l_jukebox_reply);
            return;
        }

        if (l_final_song != "~stop.mp3") {
            // We might have an aliased song. We check for its real songname and send it to the clients.
            QPair<QString, float> l_song = m_music_manager->songInformation(l_final_song, m_current_area);
            l_final_song = l_song.first;
        }
        AOPacket l_music_change("MC", {l_final_song, argv[1], m_showname, "1", "0", l_effects});
        server->broadcast(l_music_change, m_current_area);

        // Since we can't ensure a user has their showname set, we check if its empty to prevent
        //"played by ." in /currentmusic.
        if (m_showname.isEmpty()) {
            area->changeMusic(m_current_char, l_final_song);
            return;
        }
        area->changeMusic(m_showname, l_final_song);
        return;
    }

    for (int i = 0; i < server->getAreaCount(); i++) {
        QString l_area = server->getAreaName(i);
        if (l_area == l_argument) {
            changeArea(i);
            break;
        }
    }
}

void AOClient::pktWtCe(AreaData *area, int argc, QStringList argv, AOPacket packet)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    if (m_is_wtce_blocked) {
        sendServerMessage("You are blocked from using the judge controls.");
        return;
    }

    if (!area->isWtceAllowed()) {
        sendServerMessage("WTCE animations have been disabled in this area.");
        return;
    }

    if (QDateTime::currentDateTime().toSecsSinceEpoch() - m_last_wtce_time <= 5)
        return;
    m_last_wtce_time = QDateTime::currentDateTime().toSecsSinceEpoch();
    server->broadcast(packet, m_current_area);
    updateJudgeLog(area, this, "WT/CE");
}

void AOClient::pktHpBar(AreaData *area, int argc, QStringList argv, AOPacket packet)
{
    Q_UNUSED(argc);
    Q_UNUSED(packet);

    if (m_is_wtce_blocked) {
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

void AOClient::pktModCall(AreaData *area, int argc, QStringList argv, AOPacket packet)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QString l_name = m_ooc_name;
    if (m_ooc_name.isEmpty())
        l_name = m_current_char;

    QString l_areaName = area->name();

    QString l_modcallNotice = "!!!MODCALL!!!\nArea: " + l_areaName + "\nCaller: " + l_name + "\n";

    if (!packet.getContent()[0].isEmpty())
        l_modcallNotice.append("Reason: " + packet.getContent()[0]);
    else
        l_modcallNotice.append("No reason given.");

    const QVector<AOClient *> l_clients = server->getClients();
    for (AOClient *l_client : l_clients) {
        if (l_client->m_authenticated)
            l_client->sendPacket(AOPacket("ZZ", {l_modcallNotice}));
    }
    emit logModcall((m_current_char + " " + m_showname), m_ipid, m_ooc_name, server->getAreaById(m_current_area)->name());

    if (ConfigManager::discordModcallWebhookEnabled()) {
        QString l_name = m_ooc_name;
        if (m_ooc_name.isEmpty())
            l_name = m_current_char;

        QString l_areaName = area->name();
        emit server->modcallWebhookRequest(l_name, l_areaName, packet.getContent().value(0), server->getAreaBuffer(l_areaName));
    }
}

void AOClient::pktAddEvidence(AreaData *area, int argc, QStringList argv, AOPacket packet)
{
    Q_UNUSED(argc);
    Q_UNUSED(packet);

    if (!checkEvidenceAccess(area))
        return;
    AreaData::Evidence l_evi = {argv[0], argv[1], argv[2]};
    area->appendEvidence(l_evi);
    sendEvidenceList(area);
}

void AOClient::pktRemoveEvidence(AreaData *area, int argc, QStringList argv, AOPacket packet)
{
    Q_UNUSED(argc);
    Q_UNUSED(packet);

    if (!checkEvidenceAccess(area))
        return;
    bool is_int = false;
    int l_idx = argv[0].toInt(&is_int);
    if (is_int && l_idx < area->evidence().size() && l_idx >= 0) {
        area->deleteEvidence(l_idx);
    }
    sendEvidenceList(area);
}

void AOClient::pktEditEvidence(AreaData *area, int argc, QStringList argv, AOPacket packet)
{
    Q_UNUSED(argc);
    Q_UNUSED(packet);

    if (!checkEvidenceAccess(area))
        return;
    bool is_int = false;
    int l_idx = argv[0].toInt(&is_int);
    AreaData::Evidence l_evi = {argv[1], argv[2], argv[3]};
    if (is_int && l_idx < area->evidence().size() && l_idx >= 0) {
        area->replaceEvidence(l_idx, l_evi);
    }
    sendEvidenceList(area);
}

void AOClient::pktSetCase(AreaData *area, int argc, QStringList argv, AOPacket packet)
{
    Q_UNUSED(area);
    Q_UNUSED(argc);
    Q_UNUSED(packet);

    QList<bool> l_prefs_list;
    for (int i = 2; i <= 6; i++) {
        bool is_int = false;
        bool pref = argv[i].toInt(&is_int);
        if (!is_int)
            return;
        l_prefs_list.append(pref);
    }
    m_casing_preferences = l_prefs_list;
}

void AOClient::pktAnnounceCase(AreaData *area, int argc, QStringList argv, AOPacket packet)
{
    Q_UNUSED(area);
    Q_UNUSED(argc);
    Q_UNUSED(packet);

    QString l_case_title = argv[0];
    QStringList l_needed_roles;
    QList<bool> l_needs_list;
    for (int i = 1; i <= 5; i++) {
        bool is_int = false;
        bool need = argv[i].toInt(&is_int);
        if (!is_int)
            return;
        l_needs_list.append(need);
    }
    QStringList l_roles = {"defense attorney", "prosecutor", "judge", "jurors", "stenographer"};
    for (int i = 0; i < 5; i++) {
        if (l_needs_list[i])
            l_needed_roles.append(l_roles[i]);
    }
    if (l_needed_roles.isEmpty())
        return;

    QString l_message = "=== Case Announcement ===\r\n" + (m_ooc_name == "" ? m_current_char : m_ooc_name) + " needs " + l_needed_roles.join(", ") + " for " + (l_case_title == "" ? "a case" : l_case_title) + "!";

    QList<AOClient *> l_clients_to_alert;
    // here lies morton, RIP
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    QSet<bool> l_needs_set(l_needs_list.begin(), l_needs_list.end());
#else
    QSet<bool> l_needs_set = l_needs_list.toSet();
#endif
    const QVector<AOClient *> l_clients = server->getClients();
    for (AOClient *l_client : l_clients) {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
        QSet<bool> l_matches(l_client->m_casing_preferences.begin(), l_client->m_casing_preferences.end());
        l_matches.intersect(l_needs_set);
#else
        QSet<bool> l_matches = l_client->m_casing_preferences.toSet().intersect(l_needs_set);
#endif
        if (!l_matches.isEmpty() && !l_clients_to_alert.contains(l_client))
            l_clients_to_alert.append(l_client);
    }

    for (AOClient *l_client : l_clients_to_alert) {
        l_client->sendPacket(AOPacket("CASEA", {l_message, argv[1], argv[2], argv[3], argv[4], argv[5], "1"}));
        // you may be thinking, "hey wait a minute the network protocol documentation doesn't mention that last argument!"
        // if you are in fact thinking that, you are correct! it is not in the documentation!
        // however for some inscrutable reason Attorney Online 2 will outright reject a CASEA packet that does not have
        // at least 7 arguments despite only using the first 6. Cera, i kneel. you have truly broken me.
    }
}*/

void AOClient::sendEvidenceList(AreaData *area)
{
    const QVector<AOClient *> l_clients = server->getClients();
    for (AOClient *l_client : l_clients) {
        if (l_client->m_current_area == m_current_area)
            l_client->updateEvidenceList(area);
    }
}

void AOClient::updateEvidenceList(AreaData *area)
{
    QStringList l_evidence_list;
    QString l_evidence_format("%1&%2&%3");

    const QList<AreaData::Evidence> l_area_evidence = area->evidence();
    for (const AreaData::Evidence &evidence : l_area_evidence) {
        if (!checkPermission(ACLRole::CM) && area->eviMod() == AreaData::EvidenceMod::HIDDEN_CM) {
            QRegularExpression l_regex("<owner=(.*?)>");
            QRegularExpressionMatch l_match = l_regex.match(evidence.description);
            if (l_match.hasMatch()) {
                QStringList owners = l_match.captured(1).split(",");
                if (!owners.contains("all", Qt::CaseSensitivity::CaseInsensitive) && !owners.contains(m_pos, Qt::CaseSensitivity::CaseInsensitive)) {
                    continue;
                }
            }
            // no match = show it to all
        }
        l_evidence_list.append(l_evidence_format.arg(evidence.name, evidence.description, evidence.image));
    }

    sendPacket(PacketFactory::createPacket("LE", l_evidence_list));
}

/*
AOPacket AOClient::validateIcPacket(AOPacket packet)
{
    // Welcome to the super cursed server-side IC chat validation hell

    // I wanted to use enums or #defines here to make the
    // indicies of the args arrays more readable. But,
    // in typical AO fasion, the indicies for the incoming
    // and outgoing packets are different. Just RTFM.

    // This packet can be sent with a minimum required args of 15.
    // 2.6+ extensions raise this to 19, and 2.8 further raises this to 26.

    AOPacket l_invalid("INVALID", {});
    QStringList l_args;
    if (isSpectator() || m_current_char.isEmpty() || !m_joined)
        // Spectators cannot use IC
        return l_invalid;
    AreaData *area = server->getAreaById(m_current_area);
    if (area->lockStatus() == AreaData::LockStatus::SPECTATABLE && !area->invited().contains(m_id) && !checkPermission(ACLRole::BYPASS_LOCKS))
        // Non-invited players cannot speak in spectatable areas
        return l_invalid;

    QList<QVariant> l_incoming_args;
    for (const QString &l_arg : packet.getContent()) {
        l_incoming_args.append(QVariant(l_arg));
    }

    // desk modifier
    QStringList allowed_desk_mods;
    allowed_desk_mods << "chat"
                      << "0"
                      << "1"
                      << "2"
                      << "3"
                      << "4"
                      << "5";
    QString l_incoming_deskmod = l_incoming_args[0].toString();
    if (allowed_desk_mods.contains(l_incoming_deskmod)) {
        // **WARNING : THIS IS A HACK!**
        // A proper solution would be to deprecate chat as an argument on the clientside
        // instead of overwriting correct netcode behaviour on the serverside.
        if (l_incoming_deskmod == "chat") {
            l_args.append("1");
        }
        else {
            l_args.append(l_incoming_args[0].toString());
        }
    }
    else
        return l_invalid;

    // preanim
    l_args.append(l_incoming_args[1].toString());

    // char name
    if (m_current_char.toLower() != l_incoming_args[2].toString().toLower()) {
        // Selected char is different from supplied folder name
        // This means the user is INI-swapped
        if (!area->iniswapAllowed()) {
            if (!server->getCharacters().contains(l_incoming_args[2].toString(), Qt::CaseInsensitive))
                return l_invalid;
        }
        qDebug() << "INI swap detected from " << getIpid();
    }
    m_current_iniswap = l_incoming_args[2].toString();
    l_args.append(l_incoming_args[2].toString());

    // emote
    m_emote = l_incoming_args[3].toString();
    if (m_first_person)
        m_emote = "";
    l_args.append(m_emote);

    // message text
    if (l_incoming_args[4].toString().size() > ConfigManager::maxCharacters())
        return l_invalid;

    QString l_incoming_msg = dezalgo(l_incoming_args[4].toString().trimmed());
    if (!area->lastICMessage().isEmpty() && l_incoming_msg == area->lastICMessage()[4] && l_incoming_msg != "")
        return l_invalid;

    if (l_incoming_msg == "" && area->blankpostingAllowed() == false) {
        sendServerMessage("Blankposting has been forbidden in this area.");
        return l_invalid;
    }

    if (m_is_gimped) {
        QString l_gimp_message = ConfigManager::gimpList().at((genRand(1, ConfigManager::gimpList().size() - 1)));
        l_incoming_msg = l_gimp_message;
    }

    if (m_is_shaken) {
        QStringList l_parts = l_incoming_msg.split(" ");
        std::random_shuffle(l_parts.begin(), l_parts.end());
        l_incoming_msg = l_parts.join(" ");
    }

    if (m_is_disemvoweled) {
        QString l_disemvoweled_message = l_incoming_msg.remove(QRegExp("[AEIOUaeiou]"));
        l_incoming_msg = l_disemvoweled_message;
    }

    m_last_message = l_incoming_msg;
    l_args.append(l_incoming_msg);

    // side
    // this is validated clientside so w/e
    l_args.append(l_incoming_args[5].toString());
    if (m_pos != l_incoming_args[5].toString()) {
        m_pos = l_incoming_args[5].toString();
        updateEvidenceList(server->getAreaById(m_current_area));
    }

    // sfx name
    l_args.append(l_incoming_args[6].toString());

    // emote modifier
    // Now, gather round, y'all. Here is a story that is truly a microcosm of the AO dev experience.
    // If this value is a 4, it will crash the client. Why? Who knows, but it does.
    // Now here is the kicker: in certain versions, the client would incorrectly send a 4 here
    // For a long time, by configuring the client to do a zoom with a preanim, it would send 4
    // This would crash everyone else's client, and the feature had to be disabled
    // But, for some reason, nobody traced the cause of this issue for many many years.
    // The serverside fix is needed to ensure invalid values are not sent, because the client sucks
    int emote_mod = l_incoming_args[7].toInt();

    if (emote_mod == 4)
        emote_mod = 6;
    if (emote_mod != 0 && emote_mod != 1 && emote_mod != 2 && emote_mod != 5 && emote_mod != 6)
        return l_invalid;
    l_args.append(QString::number(emote_mod));

    // char id
    if (l_incoming_args[8].toInt() != m_char_id)
        return l_invalid;
    l_args.append(l_incoming_args[8].toString());

    // sfx delay
    l_args.append(l_incoming_args[9].toString());

    // objection modifier
    if (area->isShoutAllowed()) {
        if (l_incoming_args[10].toString().contains("4")) {
            // custom shout includes text metadata
            l_args.append(l_incoming_args[10].toString());
        }
        else {
            int l_obj_mod = l_incoming_args[10].toInt();
            if ((l_obj_mod < 0) || (l_obj_mod > 4)) {
                return l_invalid;
            }
            l_args.append(QString::number(l_obj_mod));
        }
    }
    else {
        if (l_incoming_args[10].toString() != "0") {
            sendServerMessage("Shouts have been disabled in this area.");
        }
        l_args.append("0");
    }

    // evidence
    int evi_idx = l_incoming_args[11].toInt();
    if (evi_idx > area->evidence().length())
        return l_invalid;
    l_args.append(QString::number(evi_idx));

    // flipping
    int l_flip = l_incoming_args[12].toInt();
    if (l_flip != 0 && l_flip != 1)
        return l_invalid;
    m_flipping = QString::number(l_flip);
    l_args.append(m_flipping);

    // realization
    int realization = l_incoming_args[13].toInt();
    if (realization != 0 && realization != 1)
        return l_invalid;
    l_args.append(QString::number(realization));

    // text color
    int text_color = l_incoming_args[14].toInt();
    if (text_color < 0 || text_color > 11)
        return l_invalid;
    l_args.append(QString::number(text_color));

    // 2.6 packet extensions
    if (l_incoming_args.length() >= 19) {
        // showname
        QString l_incoming_showname = dezalgo(l_incoming_args[15].toString().trimmed());
        if (!(l_incoming_showname == m_current_char || l_incoming_showname.isEmpty()) && !area->shownameAllowed()) {
            sendServerMessage("Shownames are not allowed in this area!");
            return l_invalid;
        }
        if (l_incoming_showname.length() > 30) {
            sendServerMessage("Your showname is too long! Please limit it to under 30 characters");
            return l_invalid;
        }

        // if the raw input is not empty but the trimmed input is, use a single space
        if (l_incoming_showname.isEmpty() && !l_incoming_args[15].toString().isEmpty())
            l_incoming_showname = " ";
        l_args.append(l_incoming_showname);
        m_showname = l_incoming_showname;

        // other char id
        // things get a bit hairy here
        // don't ask me how this works, because i don't know either
        QStringList l_pair_data = l_incoming_args[16].toString().split("^");
        m_pairing_with = l_pair_data[0].toInt();
        QString l_front_back = "";
        if (l_pair_data.length() > 1)
            l_front_back = "^" + l_pair_data[1];
        int l_other_charid = m_pairing_with;
        bool l_pairing = false;
        QString l_other_name = "0";
        QString l_other_emote = "0";
        QString l_other_offset = "0";
        QString l_other_flip = "0";
        for (int l_client_id : area->joinedIDs()) {
            AOClient *l_client = server->getClientByID(l_client_id);
            if (l_client->m_pairing_with == m_char_id && l_other_charid != m_char_id && l_client->m_char_id == m_pairing_with && l_client->m_pos == m_pos) {
                l_other_name = l_client->m_current_iniswap;
                l_other_emote = l_client->m_emote;
                l_other_offset = l_client->m_offset;
                l_other_flip = l_client->m_flipping;
                l_pairing = true;
            }
        }
        if (!l_pairing) {
            l_other_charid = -1;
            l_front_back = "";
        }
        l_args.append(QString::number(l_other_charid) + l_front_back);
        l_args.append(l_other_name);
        l_args.append(l_other_emote);

        // self offset
        m_offset = l_incoming_args[17].toString();
        // versions 2.6-2.8 cannot validate y-offset so we send them just the x-offset
        if ((m_version.release == 2) && (m_version.major == 6 || m_version.major == 7 || m_version.major == 8)) {
            QString l_x_offset = m_offset.split("&")[0];
            l_args.append(l_x_offset);
            QString l_other_x_offset = l_other_offset.split("&")[0];
            l_args.append(l_other_x_offset);
        }
        else {
            l_args.append(m_offset);
            l_args.append(l_other_offset);
        }
        l_args.append(l_other_flip);

        // immediate text processing
        int l_immediate = l_incoming_args[18].toInt();
        if (area->forceImmediate()) {
            if (l_args[7] == "1" || l_args[7] == "2") {
                l_args[7] = "0";
                l_immediate = 1;
            }
            else if (l_args[7] == "6") {
                l_args[7] = "5";
                l_immediate = 1;
            }
        }
        if (l_immediate != 1 && l_immediate != 0)
            return l_invalid;
        l_args.append(QString::number(l_immediate));
    }

    // 2.8 packet extensions
    if (l_incoming_args.length() >= 26) {
        // sfx looping
        int l_sfx_loop = l_incoming_args[19].toInt();
        if (l_sfx_loop != 0 && l_sfx_loop != 1)
            return l_invalid;
        l_args.append(QString::number(l_sfx_loop));

        // screenshake
        int l_screenshake = l_incoming_args[20].toInt();
        if (l_screenshake != 0 && l_screenshake != 1)
            return l_invalid;
        l_args.append(QString::number(l_screenshake));

        // frames shake
        l_args.append(l_incoming_args[21].toString());

        // frames realization
        l_args.append(l_incoming_args[22].toString());

        // frames sfx
        l_args.append(l_incoming_args[23].toString());

        // additive
        int l_additive = l_incoming_args[24].toInt();
        if (l_additive != 0 && l_additive != 1)
            return l_invalid;
        else if (area->lastICMessage().isEmpty()) {
            l_additive = 0;
        }
        else if (!(m_char_id == area->lastICMessage()[8].toInt())) {
            l_additive = 0;
        }
        else if (l_additive == 1) {
            l_args[4].insert(0, " ");
        }
        l_args.append(QString::number(l_additive));

        // effect
        l_args.append(l_incoming_args[25].toString());
    }

    // Testimony playback
    if (area->testimonyRecording() == AreaData::TestimonyRecording::RECORDING || area->testimonyRecording() == AreaData::TestimonyRecording::ADD) {
        if (l_args[5] != "wit")
            return AOPacket("MS", l_args);

        if (area->statement() == -1) {
            l_args[4] = "~~\\n-- " + l_args[4] + " --";
            l_args[14] = "3";
            server->broadcast(AOPacket("RT", {"testimony1"}), m_current_area);
        }
        addStatement(l_args);
    }
    else if (area->testimonyRecording() == AreaData::TestimonyRecording::UPDATE) {
        l_args = updateStatement(l_args);
    }
    else if (area->testimonyRecording() == AreaData::TestimonyRecording::PLAYBACK) {
        AreaData::TestimonyProgress l_progress;

        if (l_args[4] == ">") {
            m_pos = "wit";
            auto l_statement = area->jumpToStatement(area->statement() + 1);
            l_args = l_statement.first;
            l_progress = l_statement.second;

            if (l_progress == AreaData::TestimonyProgress::LOOPED) {
                sendServerMessageArea("Last statement reached. Looping to first statement.");
            }
        }
        if (l_args[4] == "<") {
            m_pos = "wit";
            auto l_statement = area->jumpToStatement(area->statement() - 1);
            l_args = l_statement.first;
            l_progress = l_statement.second;

            if (l_progress == AreaData::TestimonyProgress::STAYED_AT_FIRST) {
                sendServerMessage("First statement reached.");
            }
        }

        QString l_decoded_message = decodeMessage(l_args[4]); // Get rid of that pesky encoding first.
        QRegularExpression jump("(?<arrow>>)(?<int>[0,1,2,3,4,5,6,7,8,9]+)");
        QRegularExpressionMatch match = jump.match(l_decoded_message);
        if (match.hasMatch()) {
            m_pos = "wit";
            auto l_statement = area->jumpToStatement(match.captured("int").toInt());
            l_args = l_statement.first;
            l_progress = l_statement.second;

            switch (l_progress) {
            case AreaData::TestimonyProgress::LOOPED:
            {
                sendServerMessageArea("Last statement reached. Looping to first statement.");
                break;
            }
            case AreaData::TestimonyProgress::STAYED_AT_FIRST:
            {
                sendServerMessage("First statement reached.");
                Q_FALLTHROUGH();
            }
            case AreaData::TestimonyProgress::OK:
            default:
                // No need to handle.
                break;
            }
        }
    }

    return AOPacket("MS", l_args);
}*/

QString AOClient::dezalgo(QString p_text)
{
    QRegularExpression rxp("([̴̵̶̷̸̡̢̧̨̛̖̗̘̙̜̝̞̟̠̣̤̥̦̩̪̫̬̭̮̯̰̱̲̳̹̺̻̼͇͈͉͍͎̀́̂̃̄̅̆̇̈̉̊̋̌̍̎̏̐̑̒̓̔̽̾̿̀́͂̓̈́͆͊͋͌̕̚ͅ͏͓͔͕͖͙͚͐͑͒͗͛ͣͤͥͦͧͨͩͪͫͬͭͮͯ͘͜͟͢͝͞͠͡])");
    QString filtered = p_text.replace(rxp, "");
    return filtered;
}

bool AOClient::checkEvidenceAccess(AreaData *area)
{
    switch (area->eviMod()) {
    case AreaData::EvidenceMod::FFA:
        return true;
    case AreaData::EvidenceMod::CM:
    case AreaData::EvidenceMod::HIDDEN_CM:
        return checkPermission(ACLRole::CM);
    case AreaData::EvidenceMod::MOD:
        return m_authenticated;
    default:
        return false;
    }
}

void AOClient::updateJudgeLog(AreaData *area, AOClient *client, QString action)
{
    QString l_timestamp = QTime::currentTime().toString("hh:mm:ss");
    QString l_uid = QString::number(client->m_id);
    QString l_char_name = client->m_current_char;
    QString l_ipid = client->getIpid();
    QString l_message = action;
    QString l_logmessage = QString("[%1]: [%2] %3 (%4) %5").arg(l_timestamp, l_uid, l_char_name, l_ipid, l_message);
    area->appendJudgelog(l_logmessage);
}

QString AOClient::decodeMessage(QString incoming_message)
{
    QString decoded_message = incoming_message.replace("<num>", "#")
                                  .replace("<percent>", "%")
                                  .replace("<dollar>", "$")
                                  .replace("<and>", "&");
    return decoded_message;
}

void AOClient::loginAttempt(QString message)
{
    switch (ConfigManager::authType()) {
    case DataTypes::AuthType::SIMPLE:
        if (message == ConfigManager::modpass()) {
            sendPacket("AUTH", {"1"});                      // Client: "You were granted the Disable Modcalls button."
            sendServerMessage("Logged in as a moderator."); // pre-2.9.1 clients are hardcoded to display the mod UI when this string is sent in OOC
            m_authenticated = true;
            m_acl_role_id = ACLRolesHandler::SUPER_ID;
        }
        else {
            sendPacket("AUTH", {"0"}); // Client: "Login unsuccessful."
            sendServerMessage("Incorrect password.");
        }
        emit logLogin((m_current_char + " " + m_showname), m_ooc_name, "Moderator",
                      m_ipid, server->getAreaById(m_current_area)->name(), m_authenticated);
        break;
    case DataTypes::AuthType::ADVANCED:
        QStringList l_login = message.split(" ");
        if (l_login.size() < 2) {
            sendServerMessage("You must specify a username and a password");
            sendServerMessage("Exiting login prompt.");
            m_is_logging_in = false;
            return;
        }
        QString username = l_login[0];
        QString password = l_login[1];
        if (server->getDatabaseManager()->authenticate(username, password)) {
            m_authenticated = true;
            m_acl_role_id = server->getDatabaseManager()->getACL(username);
            m_moderator_name = username;
            sendPacket("AUTH", {"1"}); // Client: "You were granted the Disable Modcalls button."
            if (m_version.release <= 2 && m_version.major <= 9 && m_version.minor <= 0)
                sendServerMessage("Logged in as a moderator."); // pre-2.9.1 clients are hardcoded to display the mod UI when this string is sent in OOC
            sendServerMessage("Welcome, " + username);
        }
        else {
            sendPacket("AUTH", {"0"}); // Client: "Login unsuccessful."
            sendServerMessage("Incorrect password.");
        }
        emit logLogin((m_current_char + " " + m_showname), m_ooc_name, username, m_ipid,
                      server->getAreaById(m_current_area)->name(), m_authenticated);
        break;
    }
    sendServerMessage("Exiting login prompt.");
    m_is_logging_in = false;
    return;
}
