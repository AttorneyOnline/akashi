//////////////////////////////////////////////////////////////////////////////////////
//    akashi - a server for Attorney Online 2                                       //
//    Copyright (C) 2020  scatterflower                                             //
//                                                                                  //
//    This program is free software: you can redistribute it and/or modify          //
//    it under the terms of the GNU Affero General Public License as                //
//    published by the Free Software Foundation, either version 3 of the            //
//    License, or (at your option) any later version.                               //
//                                                                                  //
//    This program is distributed in the hope that it will be useful,               //
//    but WITHOUT ANY WARRANTY; without even the implied warranty of                //
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                 //
//    GNU Affero General Public License for more details.                           //
//                                                                                  //
//    You should have received a copy of the GNU Affero General Public License      //
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.        //
//////////////////////////////////////////////////////////////////////////////////////
#include "aoclient.h"

#include "proto/packet.h"

#include "area_data.h"
#include "command_extension.h"
#include "config_manager.h"
#include "db_manager.h"
#include "packet/packet_factory.h"
#include "playerstateobserver.h"
#include "server.h"

const QMap<QString, AOClient::CommandInfo> AOClient::COMMANDS{
    {"login", {{ACLRole::NONE}, 0, &AOClient::cmdLogin}},
    {"getarea", {{ACLRole::NONE}, 0, &AOClient::cmdGetArea}},
    {"getareas", {{ACLRole::NONE}, 0, &AOClient::cmdGetAreas}},
    {"ban", {{ACLRole::BAN}, 3, &AOClient::cmdBan}},
    {"kick", {{ACLRole::KICK}, 2, &AOClient::cmdKick}},
    {"changeauth", {{ACLRole::SUPER}, 0, &AOClient::cmdChangeAuth}},
    {"rootpass", {{ACLRole::SUPER}, 1, &AOClient::cmdSetRootPass}},
    {"background", {{ACLRole::NONE}, 1, &AOClient::cmdSetBackground}},
    {"side", {{ACLRole::CM}, 0, &AOClient::cmdSetSide}},
    {"lock_background", {{ACLRole::CM}, 0, &AOClient::cmdBgLock}},
    {"unlock_background", {{ACLRole::CM}, 0, &AOClient::cmdBgUnlock}},
    {"adduser", {{ACLRole::MODIFY_USERS}, 2, &AOClient::cmdAddUser}},
    {"removeuser", {{ACLRole::MODIFY_USERS}, 1, &AOClient::cmdRemoveUser}},
    {"listusers", {{ACLRole::MODIFY_USERS}, 0, &AOClient::cmdListUsers}},
    {"setperms", {{ACLRole::MODIFY_USERS}, 2, &AOClient::cmdSetPerms}},
    {"removeperms", {{ACLRole::MODIFY_USERS}, 1, &AOClient::cmdRemovePerms}},
    {"listperms", {{ACLRole::NONE}, 0, &AOClient::cmdListPerms}},
    {"logout", {{ACLRole::NONE}, 0, &AOClient::cmdLogout}},
    {"pos", {{ACLRole::NONE}, 1, &AOClient::cmdPos}},
    {"g", {{ACLRole::NONE}, 1, &AOClient::cmdG}},
    {"need", {{ACLRole::NONE}, 1, &AOClient::cmdNeed}},
    {"coinflip", {{ACLRole::NONE}, 0, &AOClient::cmdFlip}},
    {"roll", {{ACLRole::NONE}, 0, &AOClient::cmdRoll}},
    {"rolla", {{ACLRole::NONE}, 0, &AOClient::cmdRollA}},
    {"rollp", {{ACLRole::NONE}, 0, &AOClient::cmdRollP}},
    {"doc", {{ACLRole::NONE}, 0, &AOClient::cmdDoc}},
    {"cleardoc", {{ACLRole::NONE}, 0, &AOClient::cmdClearDoc}},
    {"cm", {{ACLRole::NONE}, 0, &AOClient::cmdCM}},
    {"uncm", {{ACLRole::CM}, 0, &AOClient::cmdUnCM}},
    {"invite", {{ACLRole::CM}, 1, &AOClient::cmdInvite}},
    {"uninvite", {{ACLRole::CM}, 1, &AOClient::cmdUnInvite}},
    {"area_lock", {{ACLRole::CM}, 0, &AOClient::cmdLock}},
    {"area_spectate", {{ACLRole::CM}, 0, &AOClient::cmdSpectatable}},
    {"area_unlock", {{ACLRole::CM}, 0, &AOClient::cmdUnLock}},
    {"timer", {{ACLRole::CM}, 0, &AOClient::cmdTimer}},
    {"area", {{ACLRole::NONE}, 1, &AOClient::cmdArea}},
    {"play", {{ACLRole::NONE}, 1, &AOClient::cmdPlay}},
    {"area_kick", {{ACLRole::CM}, 1, &AOClient::cmdAreaKick}},
    {"randomchar", {{ACLRole::NONE}, 0, &AOClient::cmdRandomChar}},
    {"switch", {{ACLRole::NONE}, 1, &AOClient::cmdSwitch}},
    {"toggleglobal", {{ACLRole::NONE}, 0, &AOClient::cmdToggleGlobal}},
    {"mods", {{ACLRole::NONE}, 0, &AOClient::cmdMods}},
    {"commands", {{ACLRole::NONE}, 0, &AOClient::cmdCommands}},
    {"status", {{ACLRole::NONE}, 1, &AOClient::cmdStatus}},
    {"forcepos", {{ACLRole::CM}, 2, &AOClient::cmdForcePos}},
    {"currentmusic", {{ACLRole::NONE}, 0, &AOClient::cmdCurrentMusic}},
    {"pm", {{ACLRole::NONE}, 2, &AOClient::cmdPM}},
    {"evidence_mod", {{ACLRole::EVI_MOD}, 1, &AOClient::cmdEvidenceMod}},
    {"motd", {{ACLRole::NONE}, 0, &AOClient::cmdMOTD}},
    {"set_motd", {{ACLRole::MOTD}, 1, &AOClient::cmdSetMOTD}},
    {"announce", {{ACLRole::ANNOUNCE}, 1, &AOClient::cmdAnnounce}},
    {"m", {{ACLRole::MODCHAT}, 1, &AOClient::cmdM}},
    {"gm", {{ACLRole::MODCHAT}, 1, &AOClient::cmdGM}},
    {"mute", {{ACLRole::MUTE}, 1, &AOClient::cmdMute}},
    {"unmute", {{ACLRole::MUTE}, 1, &AOClient::cmdUnMute}},
    {"bans", {{ACLRole::BAN}, 0, &AOClient::cmdBans}},
    {"unban", {{ACLRole::BAN}, 1, &AOClient::cmdUnBan}},
    {"subtheme", {{ACLRole::CM}, 1, &AOClient::cmdSubTheme}},
    {"about", {{ACLRole::NONE}, 0, &AOClient::cmdAbout}},
    {"evidence_swap", {{ACLRole::CM}, 2, &AOClient::cmdEvidence_Swap}},
    {"notecard", {{ACLRole::NONE}, 1, &AOClient::cmdNoteCard}},
    {"notecard_reveal", {{ACLRole::CM}, 0, &AOClient::cmdNoteCardReveal}},
    {"notecard_clear", {{ACLRole::NONE}, 0, &AOClient::cmdNoteCardClear}},
    {"8ball", {{ACLRole::NONE}, 1, &AOClient::cmd8Ball}},
    {"lm", {{ACLRole::MODCHAT}, 1, &AOClient::cmdLM}},
    {"judgelog", {{ACLRole::CM}, 0, &AOClient::cmdJudgeLog}},
    {"allow_blankposting", {{ACLRole::MODCHAT}, 0, &AOClient::cmdAllowBlankposting}},
    {"gimp", {{ACLRole::MUTE}, 1, &AOClient::cmdGimp}},
    {"ungimp", {{ACLRole::MUTE}, 1, &AOClient::cmdUnGimp}},
    {"baninfo", {{ACLRole::BAN}, 1, &AOClient::cmdBanInfo}},
    {"testify", {{ACLRole::CM}, 0, &AOClient::cmdTestify}},
    {"testimony", {{ACLRole::NONE}, 0, &AOClient::cmdTestimony}},
    {"examine", {{ACLRole::CM}, 0, &AOClient::cmdExamine}},
    {"pause", {{ACLRole::CM}, 0, &AOClient::cmdPauseTestimony}},
    {"delete", {{ACLRole::CM}, 0, &AOClient::cmdDeleteStatement}},
    {"update", {{ACLRole::CM}, 0, &AOClient::cmdUpdateStatement}},
    {"add", {{ACLRole::CM}, 0, &AOClient::cmdAddStatement}},
    {"reload", {{ACLRole::SUPER}, 0, &AOClient::cmdReload}},
    {"disemvowel", {{ACLRole::MUTE}, 1, &AOClient::cmdDisemvowel}},
    {"undisemvowel", {{ACLRole::MUTE}, 1, &AOClient::cmdUnDisemvowel}},
    {"shake", {{ACLRole::MUTE}, 1, &AOClient::cmdShake}},
    {"unshake", {{ACLRole::MUTE}, 1, &AOClient::cmdUnShake}},
    {"force_noint_pres", {{ACLRole::CM}, 0, &AOClient::cmdForceImmediate}},
    {"allow_iniswap", {{ACLRole::CM}, 0, &AOClient::cmdAllowIniswap}},
    {"afk", {{ACLRole::NONE}, 0, &AOClient::cmdAfk}},
    {"savetestimony", {{ACLRole::NONE}, 1, &AOClient::cmdSaveTestimony}},
    {"loadtestimony", {{ACLRole::CM}, 1, &AOClient::cmdLoadTestimony}},
    {"permitsaving", {{ACLRole::MODCHAT}, 1, &AOClient::cmdPermitSaving}},
    {"mutepm", {{ACLRole::NONE}, 0, &AOClient::cmdMutePM}},
    {"toggleadverts", {{ACLRole::NONE}, 0, &AOClient::cmdToggleAdverts}},
    {"ooc_mute", {{ACLRole::MUTE}, 1, &AOClient::cmdOocMute}},
    {"ooc_unmute", {{ACLRole::MUTE}, 1, &AOClient::cmdOocUnMute}},
    {"block_wtce", {{ACLRole::MUTE}, 1, &AOClient::cmdBlockWtce}},
    {"unblock_wtce", {{ACLRole::MUTE}, 1, &AOClient::cmdUnBlockWtce}},
    {"block_dj", {{ACLRole::MUTE}, 1, &AOClient::cmdBlockDj}},
    {"unblock_dj", {{ACLRole::MUTE}, 1, &AOClient::cmdUnBlockDj}},
    {"charcurse", {{ACLRole::MUTE}, 1, &AOClient::cmdCharCurse}},
    {"uncharcurse", {{ACLRole::MUTE}, 1, &AOClient::cmdUnCharCurse}},
    {"charselect", {{ACLRole::NONE}, 0, &AOClient::cmdCharSelect}},
    {"force_charselect", {{ACLRole::FORCE_CHARSELECT}, 1, &AOClient::cmdForceCharSelect}},
    {"togglemusic", {{ACLRole::CM}, 0, &AOClient::cmdToggleMusic}},
    {"a", {{ACLRole::NONE}, 2, &AOClient::cmdA}},
    {"s", {{ACLRole::NONE}, 0, &AOClient::cmdS}},
    {"kick_uid", {{ACLRole::KICK}, 2, &AOClient::cmdKickUid}},
    {"firstperson", {{ACLRole::NONE}, 0, &AOClient::cmdFirstPerson}},
    {"update_ban", {{ACLRole::BAN}, 3, &AOClient::cmdUpdateBan}},
    {"changepass", {{ACLRole::NONE}, 1, &AOClient::cmdChangePassword}},
    {"ignore_bglist", {{ACLRole::IGNORE_BGLIST}, 0, &AOClient::cmdIgnoreBgList}},
    {"notice", {{ACLRole::SEND_NOTICE}, 1, &AOClient::cmdNotice}},
    {"noticeg", {{ACLRole::SEND_NOTICE}, 1, &AOClient::cmdNoticeGlobal}},
    {"togglejukebox", {{ACLRole::CM, ACLRole::JUKEBOX}, 0, &AOClient::cmdToggleJukebox}},
    {"help", {{ACLRole::NONE}, 0, &AOClient::cmdHelp}},
    {"togglemessage", {{ACLRole::CM}, 0, &AOClient::cmdToggleAreaMessageOnJoin}},
    {"clearmessage", {{ACLRole::CM}, 0, &AOClient::cmdClearAreaMessage}},
    {"areamessage", {{ACLRole::CM}, 0, &AOClient::cmdAreaMessage}},
    {"webfiles", {{ACLRole::NONE}, 0, &AOClient::cmdWebfiles}},
    {"addmusic", {{ACLRole::CM}, 1, &AOClient::cmdAddMusic}},
    {"addmusiccategory", {{ACLRole::CM}, 1, &AOClient::cmdAddMusicCategory}},
    {"removecustommusic", {{ACLRole::CM}, 1, &AOClient::cmdRemoveCustomMusic}},
    {"togglecustommusic", {{ACLRole::CM}, 0, &AOClient::cmdToggleCustomMusic}},
    {"clearcustommusic", {{ACLRole::CM}, 0, &AOClient::cmdClearCustomMusic}},
    {"toggle_wtce", {{ACLRole::CM}, 0, &AOClient::cmdToggleWtce}},
    {"toggle_shouts", {{ACLRole::CM}, 0, &AOClient::cmdToggleShouts}},
    {"kick_other", {{ACLRole::NONE}, 0, &AOClient::cmdKickOther}},
    {"jukebox_skip", {{ACLRole::CM}, 0, &AOClient::cmdJukeboxSkip}},
    {"play_ambience", {{ACLRole::NONE}, 1, &AOClient::cmdPlayAmbience}},
    {"medieval", {{ACLRole::MUTE}, 1, &AOClient::cmdMedieval}},
    {"unmedieval", {{ACLRole::MUTE}, 1, &AOClient::cmdUnMedieval}},
    {"medievalmode", {{ACLRole::MUTE}, 0, &AOClient::cmdMedievalMode}},
};

void AOClient::clientDisconnected()
{
#ifdef NET_DEBUG
    qDebug() << m_remote_ip.toString() << "disconnected";
#endif
    if (m_joined) {
        server->getAreaById(areaId())->removeClient(server->getCharID(character()), clientId());
        arup(ARUPType::PLAYER_COUNT, true);
    }

    if (character() != "") {
        server->updateCharsTaken(server->getAreaById(areaId()));
    }

    bool l_updateLocks = false;

    const QVector<AreaData *> l_areas = server->getAreas();
    for (AreaData *l_area : l_areas) {
        if (l_area->invited().contains(m_id)) {
            l_area->uninvite(m_id);
        }

        l_updateLocks = l_updateLocks || l_area->removeOwner(clientId());
    }

    if (l_updateLocks) {
        arup(ARUPType::LOCKED, true);
    }
    arup(ARUPType::CM, true);
    Q_EMIT clientSuccessfullyDisconnected(clientId());
}

void AOClient::handlePacket(const akashi::Packet &packet)
{
#ifdef NET_DEBUG
    qDebug() << "Received packet:" << packet.header() << ":" << packet.fields() << "args length:" << packet.fieldCount();
#endif

    qint64 current_tick = QDateTime::currentSecsSinceEpoch();
    if (rate_limit_tick < current_tick) {
        rate_limit_tick = current_tick;
        packet_count = 0;
    }

    ++packet_count;
    int hard_limit = ConfigManager::packetRateLimitHard();
    int soft_limit = ConfigManager::packetRateLimitSoft();

    if (hard_limit > 0 && packet_count >= hard_limit) {
        sendPacket("BD", {"You have been disconnected for sending messages too quickly."});
        m_socket->close();
        return;
    }
    else if (soft_limit > 0 && packet_count >= soft_limit) {
        sendServerMessage("You are sending messages too quickly. Please slow down.");
    }

    // Unreadable data still counts against the rate limit above.
    if (packet.isNull()) {
        return;
    }

    if (packet.fields().join("").size() > 16384) {
        return;
    }

    if (m_packets) {
        if (const auto l_spec = m_packets->handlers().spec(packet.header())) {
            handleRegisteredPacket(packet, *l_spec);
            return;
        }
    }

    // Packet families that have not moved to the registry yet go the old way.
    const std::unique_ptr<AOPacket> l_packet(PacketFactory::createPacket(packet.header(), packet.fields()));
    AreaData *l_area = server->getAreaById(areaId());

    if (!checkPermission(l_packet->getPacketInfo().acl_permission)) {
        return;
    }

    resetAfk(packet.header());

    if (l_packet->getContent().length() < l_packet->getPacketInfo().min_args) {
#ifdef NET_DEBUG
        qDebug() << "Invalid packet args length. Minimum is" << l_packet->getPacketInfo().min_args << "but only" << l_packet->getContent().length() << "were given.";
#endif
        return;
    }

    l_packet->handlePacket(l_area, *this);
}

void AOClient::handleRegisteredPacket(const akashi::Packet &f_packet, const akashi::PacketSpec &f_spec)
{
    ACLRole::Permission l_permission = ACLRole::NONE;
    if (!f_spec.required_permission.isEmpty()) {
        l_permission = ACLRole::PERMISSION_CAPTIONS.key(f_spec.required_permission, ACLRole::NONE);
    }
    if (!checkPermission(l_permission)) {
        return;
    }

    resetAfk(f_packet.header());

    if (f_packet.fieldCount() < f_spec.min_args) {
        return;
    }

    const std::shared_ptr<akashi::Codec> l_codec = m_codecs.codecFor(f_packet.header());
    if (!l_codec) {
        return;
    }
    const std::unique_ptr<akashi::Message> l_message = l_codec->decode(f_packet);
    if (!l_message) {
        return;
    }
    m_packets->handlers().handler(f_packet.header())->handle(*l_message, *this);
}

void AOClient::resetAfk(const QString &f_header)
{
    if (f_header != "CH" && m_joined) {
        if (m_is_afk) {
            sendServerMessage("You are no longer AFK.");
        }
        m_is_afk = false;
        if (characterName().endsWith(" [AFK]")) {
            setCharacterName(characterName().remove(" [AFK]"));
        }
        m_afk_timer->start(ConfigManager::afkTimeout() * 1000);
    }
}

void AOClient::changeArea(int new_area)
{
    if (areaId() == new_area) {
        sendServerMessage("You are already in area " + server->getAreaName(areaId()));
        return;
    }
    if (server->getAreaById(new_area)->lockStatus() == AreaData::LockStatus::LOCKED && !server->getAreaById(new_area)->invited().contains(clientId()) && !checkPermission(ACLRole::BYPASS_LOCKS)) {
        sendServerMessage("Area " + server->getAreaName(new_area) + " is locked.");
        return;
    }

    if (character() != "") {
        server->getAreaById(areaId())->changeCharacter(server->getCharID(character()), -1);
        server->updateCharsTaken(server->getAreaById(areaId()));
    }
    server->getAreaById(areaId())->removeClient(m_char_id, clientId());
    bool l_character_taken = false;
    if (server->getAreaById(new_area)->charactersTaken().contains(server->getCharID(character()))) {
        setCharacter("");
        m_char_id = -1;
        l_character_taken = true;
    }
    server->getAreaById(new_area)->addClient(m_char_id, clientId());
    setAreaId(new_area);
    arup(ARUPType::PLAYER_COUNT, true);
    sendEvidenceList(server->getAreaById(new_area));
    sendPacket("HP", {"1", QString::number(server->getAreaById(new_area)->defHP())});
    sendPacket("HP", {"2", QString::number(server->getAreaById(new_area)->proHP())});
    sendPacket("BN", {server->getAreaById(new_area)->background(), server->getAreaById(new_area)->side()});
    if (l_character_taken) {
        sendPacket("DONE");
    }
    const QList<QTimer *> l_timers = server->getAreaById(areaId())->timers();
    for (QTimer *l_timer : l_timers) {
        int l_timer_id = server->getAreaById(areaId())->timers().indexOf(l_timer) + 1;
        if (l_timer->isActive()) {
            sendPacket("TI", {QString::number(l_timer_id), "2"});
            sendPacket("TI", {QString::number(l_timer_id), "0", QString::number(QTime(0, 0).msecsTo(QTime(0, 0).addMSecs(l_timer->remainingTime())))});
        }
        else {
            sendPacket("TI", {QString::number(l_timer_id), "3"});
        }
    }
    sendServerMessage("You moved to area " + server->getAreaName(areaId()));
    if (server->getAreaById(areaId())->sendAreaMessageOnJoin()) {
        sendServerMessage(server->getAreaById(areaId())->areaMessage());
    }

    if (server->getAreaById(areaId())->lockStatus() == AreaData::LockStatus::SPECTATABLE) {
        sendServerMessage("Area " + server->getAreaName(areaId()) + " is spectate-only; to chat IC you will need to be invited by the CM.");
    }
}

bool AOClient::changeCharacter(int char_id)
{
    AreaData *l_area = server->getAreaById(areaId());

    if (char_id >= server->getCharacterCount()) {
        return false;
    }

    if (m_is_charcursed && !m_charcurse_list.contains(char_id)) {
        return false;
    }

    bool l_successfulChange = l_area->changeCharacter(server->getCharID(character()), char_id);

    if (char_id < 0) {
        setCharacter("");
        m_char_id = char_id;
        setSpectator(true);
    }

    if (l_successfulChange == true) {
        QString l_char_selected = server->getCharacterById(char_id);
        setCharacter(l_char_selected);
        m_pos = "";
        server->updateCharsTaken(l_area);
        sendPacket("PV", {QString::number(clientId()), "CID", QString::number(char_id)});
        return true;
    }
    return false;
}

void AOClient::changePosition(QString new_pos)
{
    m_pos = new_pos;
    sendServerMessage("Position changed to " + m_pos + ".");
    sendPacket("SP", {m_pos});
}

void AOClient::handleCommand(QString command, int argc, QStringList argv)
{
    command = command.toLower();
    QString l_target_command = command;
    QVector<ACLRole::Permission> l_permissions;

    // check for aliases
    const QList<CommandExtension> l_extensions = server->getCommandExtensionCollection()->getExtensions();
    for (const CommandExtension &i_extension : l_extensions) {
        if (i_extension.checkCommandNameAndAlias(command)) {
            l_target_command = i_extension.getCommandName();
            l_permissions = i_extension.getPermissions();
            break;
        }
    }

    CommandInfo l_command = COMMANDS.value(l_target_command, {{ACLRole::NONE}, -1, &AOClient::cmdDefault});
    if (l_permissions.isEmpty()) {
        l_permissions.append(l_command.acl_permissions);
    }

    bool l_has_permissions = false;
    for (const ACLRole::Permission i_permission : qAsConst(l_permissions)) {
        if (checkPermission(i_permission)) {
            l_has_permissions = true;
            break;
        }
    }
    if (!l_has_permissions) {
        sendServerMessage("You do not have permission to use that command.");
        return;
    }

    if (argc < l_command.minArgs) {
        sendServerMessage("Invalid command syntax.");
        sendServerMessage("The expected syntax for this command is: \n" + ConfigManager::commandHelp(command).usage);
        return;
    }

    (this->*(l_command.action))(argc, argv);
}

void AOClient::arup(ARUPType type, bool broadcast)
{
    QStringList l_arup_data;
    l_arup_data.append(QString::number(type));
    const QVector<AreaData *> l_areas = server->getAreas();
    for (AreaData *l_area : l_areas) {
        switch (type) {
        case ARUPType::PLAYER_COUNT:
        {
            l_arup_data.append(QString::number(l_area->playerCount()));
            break;
        }
        case ARUPType::STATUS:
        {
            QString l_area_status = QVariant::fromValue(l_area->status()).toString().replace("_", "-"); // LOOKING_FOR_PLAYERS to LOOKING-FOR-PLAYERS
            l_arup_data.append(l_area_status);
            break;
        }
        case ARUPType::CM:
        {
            if (l_area->owners().isEmpty()) {
                l_arup_data.append("FREE");
            }
            else {
                QStringList l_area_owners;
                const QList<int> l_owner_ids = l_area->owners();
                for (int l_owner_id : l_owner_ids) {
                    AOClient *l_owner = server->getClientByID(l_owner_id);
                    l_area_owners.append("[" + QString::number(l_owner->clientId()) + "] " + l_owner->character());
                }
                l_arup_data.append(l_area_owners.join(", "));
            }
            break;
        }
        case ARUPType::LOCKED:
        {
            QString l_lock_status = QVariant::fromValue(l_area->lockStatus()).toString();
            l_arup_data.append(l_lock_status);
            break;
        }
        default:
        {
            return;
        }
        }
    }
    if (broadcast) {
        server->broadcast(akashi::Packet("ARUP", l_arup_data));
    }
    else {
        sendPacket("ARUP", l_arup_data);
    }
}

void AOClient::fullArup()
{
    arup(ARUPType::PLAYER_COUNT, false);
    arup(ARUPType::STATUS, false);
    arup(ARUPType::CM, false);
    arup(ARUPType::LOCKED, false);
}

void AOClient::sendPacket(const akashi::Packet &packet)
{
    m_socket->write(packet);
}

void AOClient::sendPacket(QString header, QStringList contents)
{
    sendPacket(akashi::Packet(header, contents));
}

void AOClient::sendPacket(QString header)
{
    sendPacket(akashi::Packet(header));
}

void AOClient::calculateIpid()
{
    // TODO: add support for longer ipids?
    // This reduces the (fairly high) chance of
    // birthday paradox issues arising. However,
    // typing more than 8 characters might be a
    // bit cumbersome.

    QCryptographicHash hash(QCryptographicHash::Md5); // Don't need security, just hashing for uniqueness

    hash.addData(m_remote_ip.toString().toUtf8());

    m_ipid = hash.result().toHex().right(8); // Use the last 8 characters (4 bytes)
}

void AOClient::sendServerMessage(const QString &message)
{
    sendPacket("CT", {ConfigManager::serverNickname(), message, "1"});
}

void AOClient::sendServerMessageArea(QString message)
{
    server->broadcast(akashi::Packet("CT", {ConfigManager::serverNickname(), message, "1"}), areaId());
}

void AOClient::sendServerBroadcast(QString message)
{
    server->broadcast(akashi::Packet("CT", {ConfigManager::serverNickname(), message, "1"}));
}

bool AOClient::checkPermission(ACLRole::Permission f_permission) const
{
    if (f_permission == ACLRole::NONE) {
        return true;
    }

    if ((f_permission == ACLRole::CM) && server->getAreaById(areaId())->owners().contains(clientId())) {
        return true; // I'm sorry for this hack.
    }

    if (!isAuthenticated()) {
        return false;
    }

    if (ConfigManager::authType() == DataTypes::AuthType::SIMPLE) {
        return true;
    }

    const ACLRole l_role = server->getACLRolesHandler()->getRoleById(m_acl_role_id);
    return l_role.checkPermission(f_permission);
}

QString AOClient::getIpid() const
{
    return m_ipid;
}

QString AOClient::getHwid() const
{
    return m_hwid;
}

bool AOClient::isJoined() const
{
    return m_joined;
}

bool AOClient::isAuthenticated() const
{
    return m_authenticated;
}

Server *AOClient::getServer()
{
    return server;
}

int AOClient::clientId() const
{
    return m_id;
}

QString AOClient::name() const
{
    return m_ooc_name;
}

void AOClient::setName(const QString &f_name)
{
    if (f_name != m_ooc_name) {
        m_ooc_name = f_name;
        Q_EMIT nameChanged(m_ooc_name);
    }
}

int AOClient::areaId() const
{
    return m_current_area;
}

void AOClient::setAreaId(const int f_area_id)
{
    if (f_area_id != m_current_area) {
        m_current_area = f_area_id;
        Q_EMIT areaIdChanged(m_current_area);
    }
}

QString AOClient::character() const
{
    return m_current_char;
}

void AOClient::setCharacter(const QString &f_character)
{
    if (f_character != m_current_char) {
        m_current_char = f_character;
        Q_EMIT characterChanged(m_current_char);
    }
}

QString AOClient::characterName() const
{
    return m_showname;
}

void AOClient::setCharacterName(const QString &f_showname)
{
    if (f_showname != m_showname) {
        m_showname = f_showname;
        Q_EMIT characterNameChanged(m_showname);
    }
}

void AOClient::setSpectator(bool f_spectator)
{
    m_is_spectator = f_spectator;
}

bool AOClient::isSpectator() const
{
    return m_is_spectator;
}

void AOClient::onAfkTimeout()
{
    if (!m_is_afk) {
        sendServerMessage("You are now AFK.");
        setCharacterName(characterName() + " [AFK]");
    }
    m_is_afk = true;
}

AOClient::AOClient(Server *p_server, NetworkSocket *socket, QObject *parent, int user_id, MusicManager *p_manager) :
    QObject(parent),
    m_remote_ip(socket->peerAddress()),
    m_password(""),
    m_joined(false),
    m_socket(socket),
    m_music_manager(p_manager),
    m_last_wtce_time(0),
    m_id(user_id),
    m_current_area(0),
    m_current_char(""),
    server(p_server),
    rate_limit_tick(0),
    packet_count(0)
{
    m_afk_timer = new QTimer;
    m_afk_timer->setSingleShot(true);
    connect(m_afk_timer, &QTimer::timeout, this, &AOClient::onAfkTimeout);

    if (server) {
        m_packets = server->packets();
        if (m_packets) {
            // Picked again with the real profile once the client identifies.
            m_codecs = m_packets->codecs().resolve(m_profile);
        }
    }
}

AOClient::~AOClient()
{
    clientDisconnected();
    m_socket->deleteLater();
}

void AOClient::closeConnection()
{
    m_socket->close();
}

QString AOClient::hwid() const
{
    return m_hwid;
}

const akashi::ClientProfile &AOClient::profile() const
{
    return m_profile;
}

bool AOClient::isIdentified() const
{
    return m_version.release == 2;
}

void AOClient::setHwid(const QString &f_hwid)
{
    m_hwid = f_hwid;
}

void AOClient::identify(const akashi::ClientProfile &f_profile)
{
    m_profile = f_profile;
    m_version.release = f_profile.version.release;
    m_version.major = f_profile.version.major;
    m_version.minor = f_profile.version.minor;
    if (m_packets) {
        m_codecs = m_packets->codecs().resolve(m_profile);
    }
}

void AOClient::markJoined()
{
    m_joined = true;
}

void AOClient::finishJoin()
{
    Q_EMIT joined();
    server->getAreaById(areaId())->addClient(-1, clientId());
    server->getPlayerStateObserver()->registerClient(this);
}

void AOClient::logConnectionAttempt()
{
    Q_EMIT server->logConnectionAttempt(m_remote_ip.toString(), m_ipid, m_hwid);
}

std::optional<akashi::BanRecord> AOClient::hardwareBan() const
{
    const auto l_ban = server->getDatabaseManager()->isHDIDBanned(m_hwid);
    if (!l_ban.first) {
        return std::nullopt;
    }

    akashi::BanRecord l_record;
    l_record.id = l_ban.second.id;
    l_record.reason = l_ban.second.reason;
    l_record.permanent = l_ban.second.duration == -2;
    if (!l_record.permanent) {
        l_record.end = QDateTime::fromSecsSinceEpoch(l_ban.second.time).addSecs(l_ban.second.duration);
    }
    return l_record;
}

int AOClient::playerCount() const
{
    return server->getPlayerCount();
}

QStringList AOClient::characters() const
{
    return server->getCharacters();
}

QStringList AOClient::areaNames() const
{
    return server->getAreaNames();
}

QStringList AOClient::musicList() const
{
    return server->getMusicList();
}

akashi::AreaSnapshot AOClient::areaState() const
{
    AreaData *l_area = server->getAreaById(areaId());

    akashi::AreaSnapshot l_snapshot;
    l_snapshot.def_hp = l_area->defHP();
    l_snapshot.pro_hp = l_area->proHP();
    l_snapshot.background = l_area->background();
    l_snapshot.side = l_area->side();
    const QList<QTimer *> l_timers = l_area->timers();
    for (QTimer *l_timer : l_timers) {
        l_snapshot.timers.append(akashi::TimerSnapshot{l_timer->isActive(), QTime(0, 0).msecsTo(QTime(0, 0).addMSecs(l_timer->remainingTime()))});
    }
    return l_snapshot;
}

akashi::TimerSnapshot AOClient::globalTimer() const
{
    return akashi::TimerSnapshot{server->timer->isActive(), QTime(0, 0).msecsTo(QTime(0, 0).addMSecs(server->timer->remainingTime()))};
}

void AOClient::announceCharsTaken()
{
    server->updateCharsTaken(server->getAreaById(areaId()));
}

void AOClient::sendEvidenceList()
{
    sendEvidenceList(server->getAreaById(areaId()));
}

void AOClient::sendFullArup()
{
    fullArup();
}

void AOClient::broadcastPlayerCount()
{
    arup(ARUPType::PLAYER_COUNT, true);
}

bool AOClient::selectCharacter(int f_char_id)
{
    if (changeCharacter(f_char_id)) {
        m_char_id = f_char_id;
    }
    if (m_char_id > SPECTATOR_ID) {
        setSpectator(false);
    }
    return m_char_id == f_char_id;
}

bool AOClient::canUseOocChat() const
{
    return !m_is_ooc_muted;
}

QString AOClient::oocName() const
{
    return name();
}

void AOClient::setOocName(const QString &f_name)
{
    setName(f_name);
}

bool AOClient::isInLoginPrompt() const
{
    return m_is_logging_in;
}

void AOClient::attemptLogin(const QString &f_message)
{
    loginAttempt(f_message);
}

void AOClient::runCommand(const QString &f_command, const QStringList &f_arguments)
{
    handleCommand(f_command, f_arguments.size(), f_arguments);
    Q_EMIT logCMD((character() + " " + characterName()), m_ipid, name(), f_command, f_arguments, server->getAreaById(areaId())->name());
}

void AOClient::broadcastOoc(const QString &f_message)
{
    server->broadcast(akashi::Packet("CT", {name(), f_message, "0"}), areaId());
    Q_EMIT logOOC(server->getAreaById(areaId())->name(), m_ipid, name(), QString::number(clientId()), (character() + " " + characterName()), f_message);
}

bool AOClient::canModifyEvidence()
{
    return checkEvidenceAccess(server->getAreaById(areaId()));
}

bool AOClient::isEvidenceHiddenCm() const
{
    return server->getAreaById(areaId())->eviMod() == AreaData::EvidenceMod::HIDDEN_CM;
}

int AOClient::evidenceCount() const
{
    return server->getAreaById(areaId())->evidence().size();
}

void AOClient::deleteEvidence(int f_index)
{
    server->getAreaById(areaId())->deleteEvidence(f_index);
}

void AOClient::replaceEvidence(int f_index, const QString &f_name, const QString &f_description, const QString &f_image)
{
    AreaData::Evidence l_evidence;
    l_evidence.name = f_name;
    l_evidence.description = f_description;
    l_evidence.image = f_image;
    server->getAreaById(areaId())->replaceEvidence(f_index, l_evidence);
}

void AOClient::setCasingPreferences(const QList<bool> &f_preferences)
{
    m_casing_preferences = f_preferences;
}
