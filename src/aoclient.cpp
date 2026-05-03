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

#include "area_data.h"
#include "command_extension.h"
#include "config_manager.h"
#include "core/client_session.h"
#include "db_manager.h"
#include "medieval_parser.h"
#include "playerstateobserver.h"
#include "proto/ic.h"
#include "proto/packet.h"
#include "server.h"
#include "world/jukebox.h"

#include <QQueue>

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

void AOClient::leave()
{
    if (m_left) {
        return;
    }
    m_left = true;
#ifdef NET_DEBUG
    qDebug() << m_session->remote_ip.toString() << "disconnected";
#endif
    // A client that never joined has no presence to withdraw - and a rejected
    // (banned) connection must not broadcast ARUPs to the whole server.
    if (m_session->joined) {
        m_server->areaById(areaId())->removeClient(m_server->characterId(character()), clientId());
        arup(ARUPType::PLAYER_COUNT, true);

        if (character() != "") {
            m_server->updateCharsTaken(m_server->areaById(areaId()));
        }

        bool l_updateLocks = false;

        const QVector<AreaData *> l_areas = m_server->areas();
        for (AreaData *l_area : l_areas) {
            if (l_area->invited().contains(m_session->id)) {
                l_area->uninvite(m_session->id);
            }

            l_updateLocks = l_updateLocks || l_area->removeOwner(clientId());
        }

        if (l_updateLocks) {
            arup(ARUPType::LOCKED, true);
        }
        arup(ARUPType::CM, true);
    }
}

void AOClient::handlePacket(const akashi::Packet &packet)
{
#ifdef NET_DEBUG
    qDebug() << "Received packet:" << packet.header() << ":" << packet.fields() << "args length:" << packet.fieldCount();
#endif

    qint64 current_tick = QDateTime::currentSecsSinceEpoch();
    if (m_session->rate_limit_tick < current_tick) {
        m_session->rate_limit_tick = current_tick;
        m_session->packet_count = 0;
    }

    ++m_session->packet_count;
    int hard_limit = ConfigManager::packetRateLimitHard();
    int soft_limit = ConfigManager::packetRateLimitSoft();

    if (hard_limit > 0 && m_session->packet_count >= hard_limit) {
        sendPacket("BD", {"You have been disconnected for sending messages too quickly."});
        m_session->transport->close();
        return;
    }
    else if (soft_limit > 0 && m_session->packet_count >= soft_limit) {
        sendServerMessage("You are sending messages too quickly. Please slow down.");
    }

    // Unreadable data still counts against the rate limit above.
    if (packet.isNull()) {
        return;
    }

    if (packet.fields().join("").size() > 16384) {
        return;
    }

    if (m_session->packets) {
        if (const auto l_spec = m_session->packets->handlers().spec(packet.header())) {
            handleRegisteredPacket(packet, *l_spec);
        }
    }
    // A header nobody registered is dropped.
}

void AOClient::handleRegisteredPacket(const akashi::Packet &f_packet, const akashi::PacketSpec &f_spec)
{
    ACLRole::Permission l_permission = ACLRole::NONE;
    if (!f_spec.required_permission.isEmpty()) {
        l_permission = ACLRole::PERMISSION_CAPTIONS.key(f_spec.required_permission, ACLRole::NONE);
    }
    if (!canPerform(l_permission)) {
        return;
    }

    resetAfk(f_packet.header());

    if (f_packet.fieldCount() < f_spec.min_args) {
        return;
    }

    const std::shared_ptr<akashi::Codec> l_codec = m_session->codecs.codecFor(f_packet.header());
    if (!l_codec) {
        return;
    }
    const std::unique_ptr<akashi::Message> l_message = l_codec->decode(f_packet);
    if (!l_message) {
        return;
    }
    m_session->packets->handlers().handler(f_packet.header())->handle(*l_message, *this);
}

void AOClient::resetAfk(const QString &f_header)
{
    if (f_header != "CH" && m_session->joined) {
        if (m_session->afk) {
            sendServerMessage("You are no longer AFK.");
        }
        m_session->afk = false;
        if (characterName().endsWith(" [AFK]")) {
            setCharacterName(characterName().remove(" [AFK]"));
        }
        m_session->afk_timer->start(ConfigManager::afkTimeout() * 1000);
    }
}

void AOClient::changeArea(int new_area)
{
    if (areaId() == new_area) {
        sendServerMessage("You are already in area " + m_server->areaName(areaId()));
        return;
    }
    if (m_server->areaById(new_area)->lockStatus() == AreaData::LockStatus::LOCKED && !m_server->areaById(new_area)->invited().contains(clientId()) && !canPerform(ACLRole::BYPASS_LOCKS)) {
        sendServerMessage("Area " + m_server->areaName(new_area) + " is locked.");
        return;
    }

    if (character() != "") {
        m_server->areaById(areaId())->changeCharacter(m_server->characterId(character()), -1);
        m_server->updateCharsTaken(m_server->areaById(areaId()));
    }
    m_server->areaById(areaId())->removeClient(player()->char_id, clientId());
    bool l_character_taken = false;
    if (m_server->areaById(new_area)->charactersTaken().contains(m_server->characterId(character()))) {
        setCharacter("");
        player()->char_id = -1;
        l_character_taken = true;
    }
    m_server->areaById(new_area)->addClient(player()->char_id, clientId());
    setAreaId(new_area);
    arup(ARUPType::PLAYER_COUNT, true);
    sendEvidenceList(m_server->areaById(new_area));
    sendPacket("HP", {"1", QString::number(m_server->areaById(new_area)->defHP())});
    sendPacket("HP", {"2", QString::number(m_server->areaById(new_area)->proHP())});
    sendPacket("BN", {m_server->areaById(new_area)->background(), m_server->areaById(new_area)->side()});
    if (l_character_taken) {
        sendPacket("DONE");
    }
    const QList<QTimer *> l_timers = m_server->areaById(areaId())->timers();
    for (QTimer *l_timer : l_timers) {
        int l_timer_id = m_server->areaById(areaId())->timers().indexOf(l_timer) + 1;
        if (l_timer->isActive()) {
            sendPacket("TI", {QString::number(l_timer_id), "2"});
            sendPacket("TI", {QString::number(l_timer_id), "0", QString::number(QTime(0, 0).msecsTo(QTime(0, 0).addMSecs(l_timer->remainingTime())))});
        }
        else {
            sendPacket("TI", {QString::number(l_timer_id), "3"});
        }
    }
    sendServerMessage("You moved to area " + m_server->areaName(areaId()));
    if (m_server->areaById(areaId())->sendAreaMessageOnJoin()) {
        sendServerMessage(m_server->areaById(areaId())->areaMessage());
    }

    if (m_server->areaById(areaId())->lockStatus() == AreaData::LockStatus::SPECTATABLE) {
        sendServerMessage("Area " + m_server->areaName(areaId()) + " is spectate-only; to chat IC you will need to be invited by the CM.");
    }
}

bool AOClient::changeCharacter(int char_id)
{
    AreaData *l_area = m_server->areaById(areaId());

    if (char_id >= m_server->characterCount()) {
        return false;
    }

    if (isCharCursed() && !m_session->charcurse_list.contains(char_id)) {
        return false;
    }

    bool l_successfulChange = l_area->changeCharacter(m_server->characterId(character()), char_id);

    if (char_id < 0) {
        setCharacter("");
        player()->char_id = char_id;
        setSpectator(true);
    }

    if (l_successfulChange == true) {
        QString l_char_selected = m_server->characterById(char_id);
        setCharacter(l_char_selected);
        player()->pos = "";
        m_server->updateCharsTaken(l_area);
        sendPacket("PV", {QString::number(clientId()), "CID", QString::number(char_id)});
        return true;
    }
    return false;
}

void AOClient::changePosition(QString new_pos)
{
    player()->pos = new_pos;
    sendServerMessage("Position changed to " + player()->pos + ".");
    sendPacket("SP", {player()->pos});
}

void AOClient::handleCommand(QString command, int argc, QStringList argv)
{
    command = command.toLower();
    QString l_target_command = command;
    QVector<ACLRole::Permission> l_permissions;

    // check for aliases
    const QList<CommandExtension> l_extensions = m_server->commandExtensionCollection()->extensions();
    for (const CommandExtension &i_extension : l_extensions) {
        if (i_extension.checkCommandNameAndAlias(command)) {
            l_target_command = i_extension.commandName();
            l_permissions = i_extension.permissions();
            break;
        }
    }

    CommandInfo l_command = COMMANDS.value(l_target_command, {{ACLRole::NONE}, -1, &AOClient::cmdDefault});
    if (l_permissions.isEmpty()) {
        l_permissions.append(l_command.acl_permissions);
    }

    bool l_has_permissions = false;
    for (const ACLRole::Permission i_permission : qAsConst(l_permissions)) {
        if (canPerform(i_permission)) {
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
    const QVector<AreaData *> l_areas = m_server->areas();
    for (AreaData *l_area : l_areas) {
        switch (type) {
        case ARUPType::PLAYER_COUNT:
        {
            l_arup_data.append(QString::number(l_area->playerCount()));
            break;
        }
        case ARUPType::STATUS:
        {
            QString l_area_status = l_area->statusLine();
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
                    AOClient *l_owner = m_server->clientById(l_owner_id);
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
        m_server->broadcast(akashi::Packet("ARUP", l_arup_data));
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
    m_session->write(packet);
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

    hash.addData(m_session->remote_ip.toString().toUtf8());

    m_session->ipid = hash.result().toHex().right(8); // Use the last 8 characters (4 bytes)
}

void AOClient::sendServerMessage(const QString &message)
{
    sendPacket("CT", {ConfigManager::serverNickname(), message, "1"});
}

void AOClient::sendServerMessageArea(QString message)
{
    m_server->broadcast(akashi::Packet("CT", {ConfigManager::serverNickname(), message, "1"}), areaId());
}

void AOClient::sendServerBroadcast(QString message)
{
    m_server->broadcast(akashi::Packet("CT", {ConfigManager::serverNickname(), message, "1"}));
}

bool AOClient::canPerform(ACLRole::Permission f_permission) const
{
    if (f_permission == ACLRole::NONE) {
        return true;
    }

    if ((f_permission == ACLRole::CM) && m_server->areaById(areaId())->owners().contains(clientId())) {
        return true; // I'm sorry for this hack.
    }

    if (!isAuthenticated()) {
        return false;
    }

    if (ConfigManager::authType() == DataTypes::AuthType::SIMPLE) {
        return true;
    }

    const ACLRole l_role = m_server->aclRolesHandler()->roleById(m_session->acl_role_id);
    return l_role.canPerform(f_permission);
}

QString AOClient::ipid() const
{
    return m_session->ipid;
}

bool AOClient::isJoined() const
{
    return m_session->joined;
}

bool AOClient::isAuthenticated() const
{
    return m_session->authenticated;
}

Server *AOClient::server()
{
    return m_server;
}

int AOClient::clientId() const
{
    return m_session->id;
}

akashi::PlayerState *AOClient::player() const
{
    return m_session->active_player;
}

QList<akashi::PlayerState *> AOClient::players() const
{
    return m_session->players;
}

void AOClient::waitForReconnect(int f_seconds)
{
    m_session->waitForReconnect(f_seconds);
}

bool AOClient::isWaitingForReconnect() const
{
    return m_session->isWaitingForReconnect();
}

QString AOClient::name() const
{
    return player()->oocName();
}

void AOClient::setName(const QString &f_name)
{
    player()->setOocName(f_name);
}

int AOClient::areaId() const
{
    return player()->areaId();
}

void AOClient::setAreaId(const int f_area_id)
{
    player()->setAreaId(f_area_id);
}

QString AOClient::character() const
{
    return player()->character();
}

void AOClient::setCharacter(const QString &f_character)
{
    player()->setCharacter(f_character);
}

QString AOClient::characterName() const
{
    return player()->showname();
}

void AOClient::setCharacterName(const QString &f_showname)
{
    player()->setShowname(f_showname);
}

void AOClient::setSpectator(bool f_spectator)
{
    player()->spectator = f_spectator;
}

bool AOClient::isSpectator() const
{
    return player()->spectator;
}

void AOClient::onAfkTimeout()
{
    if (!m_session->afk) {
        sendServerMessage("You are now AFK.");
        setCharacterName(characterName() + " [AFK]");
    }
    m_session->afk = true;
}

// How long a connection may sit unidentified before it is dropped. A stock
// client identifies within its first second; anything silent this long is a
// dead or hostile connection squatting a player slot.
static const int IDENTIFICATION_TIMEOUT_MS = 60000;

AOClient::AOClient(Server *p_server, akashi::ITransport *socket, QObject *parent, int user_id) :
    QObject(parent),
    m_server(p_server)
{
    m_session = new akashi::ClientSession(user_id, socket, this);
    connect(m_session, &akashi::ClientSession::packetReceived, this, &AOClient::handlePacket);
    connect(m_session, &akashi::ClientSession::transportClosed, this, &AOClient::disconnected);
    connect(m_session, &akashi::ClientSession::reconnectTimedOut, this, &AOClient::reconnectTimedOut);
    connect(m_session->afk_timer, &QTimer::timeout, this, &AOClient::onAfkTimeout);

    QTimer::singleShot(IDENTIFICATION_TIMEOUT_MS, this, [this] {
        if (!m_session->identified) {
            m_session->transport->close();
        }
    });

    if (m_server) {
        m_session->packets = m_server->packets();
        if (m_session->packets) {
            // Picked again with the real profile once the client identifies.
            m_session->codecs = m_session->packets->codecs().resolve(m_session->profile);
        }
    }
}

AOClient::~AOClient()
{
    leave();
}

void AOClient::closeConnection()
{
    m_session->transport->close();
}

QString AOClient::hwid() const
{
    return m_session->hwid;
}

const akashi::ClientProfile &AOClient::profile() const
{
    return m_session->profile;
}

bool AOClient::isIdentified() const
{
    return m_session->identified;
}

void AOClient::setHwid(const QString &f_hwid)
{
    m_session->hwid = f_hwid;
}

void AOClient::identify(const akashi::ClientProfile &f_profile)
{
    // ID only learns arch and version; the features announced at connect
    // time outlive it.
    const QSet<QString> l_announced = m_session->profile.features;
    m_session->profile = f_profile;
    m_session->profile.features.unite(l_announced);
    m_session->identified = true;
    if (m_session->packets) {
        m_session->codecs = m_session->packets->codecs().resolve(m_session->profile);
    }
}

void AOClient::markJoined()
{
    m_session->joined = true;
}

void AOClient::finishJoin()
{
    Q_EMIT joined();
    m_server->areaById(areaId())->addClient(-1, clientId());
    for (akashi::PlayerState *l_player : players()) {
        m_server->playerStateObserver()->registerPlayer(l_player);
    }
}

void AOClient::logConnectionAttempt()
{
    Q_EMIT m_server->logConnectionAttempt(m_session->remote_ip.toString(), m_session->ipid, m_session->hwid);
}

std::optional<akashi::BanRecord> AOClient::hardwareBan() const
{
    const auto l_ban = m_server->databaseManager()->isHDIDBanned(m_session->hwid);
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
    return m_server->playerCount();
}

QStringList AOClient::characters() const
{
    return m_server->characters();
}

QStringList AOClient::areaNames() const
{
    return m_server->areaNames();
}

QStringList AOClient::musicList() const
{
    return m_server->musicList();
}

akashi::AreaSnapshot AOClient::areaState() const
{
    AreaData *l_area = m_server->areaById(areaId());

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
    return akashi::TimerSnapshot{m_server->timer->isActive(), QTime(0, 0).msecsTo(QTime(0, 0).addMSecs(m_server->timer->remainingTime()))};
}

void AOClient::announceCharsTaken()
{
    m_server->updateCharsTaken(m_server->areaById(areaId()));
}

void AOClient::sendEvidenceList()
{
    sendEvidenceList(m_server->areaById(areaId()));
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
        player()->char_id = f_char_id;
    }
    if (player()->char_id > SPECTATOR_ID) {
        setSpectator(false);
    }
    return player()->char_id == f_char_id;
}

bool AOClient::canUseOocChat() const
{
    return !m_session->hasSanction(akashi::Sanction::OocMute);
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
    return m_session->logging_in;
}

void AOClient::attemptLogin(const QString &f_message)
{
    loginAttempt(f_message);
}

void AOClient::runCommand(const QString &f_command, const QStringList &f_arguments)
{
    handleCommand(f_command, f_arguments.size(), f_arguments);
    Q_EMIT logCMD((character() + " " + characterName()), m_session->ipid, name(), f_command, f_arguments, m_server->areaById(areaId())->name());
}

void AOClient::broadcastOoc(const QString &f_message)
{
    m_server->broadcast(akashi::Packet("CT", {name(), f_message, "0"}), areaId());
    Q_EMIT logOOC(m_server->areaById(areaId())->name(), m_session->ipid, name(), QString::number(clientId()), (character() + " " + characterName()), f_message);
}

bool AOClient::canModifyEvidence()
{
    return canModifyEvidence(m_server->areaById(areaId()));
}

bool AOClient::isEvidenceHiddenCm() const
{
    return m_server->areaById(areaId())->eviMod() == AreaData::EvidenceMod::HIDDEN_CM;
}

int AOClient::evidenceCount() const
{
    return m_server->areaById(areaId())->evidence().size();
}

void AOClient::deleteEvidence(int f_index)
{
    AreaData *l_area = m_server->areaById(areaId());
    // The client counts the list it was shown, which in hidden mode is not
    // the whole record - every incoming evidence number must be translated
    // through this viewer's own list, the way tsu3 calculated the positions
    // by hand. A protocol update replaces this with stable item ids.
    const int l_real_index = l_area->evidenceIndexByVisibleIndex(f_index + 1, player()->pos, canPerform(ACLRole::CM));
    l_area->deleteEvidence(l_real_index);
}

void AOClient::replaceEvidence(int f_index, const QString &f_name, const QString &f_description, const QString &f_image)
{
    AreaData *l_area = m_server->areaById(areaId());
    // Same viewer translation as deleteEvidence; an unknown number maps to
    // -1 and the store refuses it, so a stale list cannot hit the wrong item.
    const int l_real_index = l_area->evidenceIndexByVisibleIndex(f_index + 1, player()->pos, canPerform(ACLRole::CM));
    AreaData::Evidence l_evidence;
    l_evidence.name = f_name;
    l_evidence.description = f_description;
    l_evidence.image = f_image;
    l_area->replaceEvidence(l_real_index, l_evidence);
}

void AOClient::setCasingPreferences(const QList<bool> &f_preferences)
{
    m_session->casing_preferences = f_preferences;
}

bool AOClient::canUseIcChat() const
{
    return !m_session->hasSanction(akashi::Sanction::Mute);
}

bool AOClient::isMuted() const
{
    return m_session->hasSanction(akashi::Sanction::Mute);
}

void AOClient::setMuted(bool f_muted)
{
    m_session->setSanction(akashi::Sanction::Mute, f_muted);
}

bool AOClient::isOocMuted() const
{
    return m_session->hasSanction(akashi::Sanction::OocMute);
}

void AOClient::setOocMuted(bool f_ooc_muted)
{
    m_session->setSanction(akashi::Sanction::OocMute, f_ooc_muted);
}

void AOClient::setDjBlocked(bool f_dj_blocked)
{
    m_session->setSanction(akashi::Sanction::DjBlock, f_dj_blocked);
}

void AOClient::setWtceBlocked(bool f_wtce_blocked)
{
    m_session->setSanction(akashi::Sanction::WtceBlock, f_wtce_blocked);
}

int AOClient::characterId() const
{
    return player()->char_id;
}

bool AOClient::isFirstPerson() const
{
    return player()->first_person;
}

void AOClient::setIniswap(const QString &f_character)
{
    player()->iniswap = f_character;
}

void AOClient::setEmote(const QString &f_emote)
{
    player()->emote = f_emote;
}

void AOClient::setOffset(const QString &f_offset)
{
    player()->offset = f_offset;
}

void AOClient::setFlipping(const QString &f_flipping)
{
    player()->flipping = f_flipping;
}

QString AOClient::iniswap() const
{
    return player()->iniswap;
}

QString AOClient::emote() const
{
    return player()->emote;
}

QString AOClient::offset() const
{
    return player()->offset;
}

QString AOClient::flipping() const
{
    return player()->flipping;
}

QString AOClient::pos() const
{
    return player()->pos;
}

int AOClient::pairingWith() const
{
    return player()->pairing_with;
}

void AOClient::setPairingWith(int f_char_id)
{
    player()->pairing_with = f_char_id;
}

QString AOClient::lastIcMessage() const
{
    return player()->last_message;
}

void AOClient::setLastIcMessage(const QString &f_message)
{
    player()->last_message = f_message;
}

void AOClient::updatePosition(const QString &f_position)
{
    if (player()->pos != f_position) {
        player()->pos = f_position;
        player()->pos.replace("../", "").replace("..\\", "");
        updateEvidenceList(m_server->areaById(areaId()));
    }
}

QString AOClient::gimpText()
{
    return ConfigManager::gimpList().at(genRand(1, ConfigManager::gimpList().size() - 1));
}

QString AOClient::medievalText(const QString &f_text)
{
    QString l_text = f_text;
    return m_server->medievalParser()->degrootify(l_text);
}

bool AOClient::isGimped() const
{
    return m_session->hasSanction(akashi::Sanction::Gimp);
}

bool AOClient::isMedieval() const
{
    return player()->medieval;
}

bool AOClient::isMedievalArea() const
{
    return m_server->areaById(areaId())->isMedievalMode();
}

bool AOClient::isShaken() const
{
    return m_session->hasSanction(akashi::Sanction::Shake);
}

bool AOClient::isDisemvoweled() const
{
    return m_session->hasSanction(akashi::Sanction::Disemvowel);
}

void AOClient::setGimped(bool f_gimped)
{
    m_session->setSanction(akashi::Sanction::Gimp, f_gimped);
}

void AOClient::setMedieval(bool f_medieval)
{
    player()->medieval = f_medieval;
}

void AOClient::setShaken(bool f_shaken)
{
    m_session->setSanction(akashi::Sanction::Shake, f_shaken);
}

void AOClient::setDisemvoweled(bool f_disemvoweled)
{
    m_session->setSanction(akashi::Sanction::Disemvowel, f_disemvoweled);
}

bool AOClient::isAfk() const
{
    return m_session->afk;
}

void AOClient::setAfk(bool f_afk)
{
    m_session->afk = f_afk;
}

bool AOClient::isPmMuted() const
{
    return m_session->pm_muted;
}

void AOClient::setPmMuted(bool f_pm_muted)
{
    m_session->pm_muted = f_pm_muted;
}

bool AOClient::isAdvertEnabled() const
{
    return m_session->advert_enabled;
}

void AOClient::setAdvertEnabled(bool f_advert_enabled)
{
    m_session->advert_enabled = f_advert_enabled;
}

bool AOClient::isCharCursed() const
{
    return m_session->hasSanction(akashi::Sanction::CharCurse);
}

void AOClient::setCharCursed(bool f_char_cursed)
{
    m_session->setSanction(akashi::Sanction::CharCurse, f_char_cursed);
}

bool AOClient::isTestimonySaving() const
{
    return m_session->testimony_saving;
}

void AOClient::setTestimonySaving(bool f_testimony_saving)
{
    m_session->testimony_saving = f_testimony_saving;
}

QHostAddress AOClient::remoteIp() const
{
    return m_session->remote_ip;
}

QString AOClient::moderatorName() const
{
    return m_session->moderator_name;
}

QString AOClient::aclRoleId() const
{
    return m_session->acl_role_id;
}

void AOClient::setInLoginPrompt(bool f_in_login_prompt)
{
    m_session->logging_in = f_in_login_prompt;
}

void AOClient::setCharacterId(int f_char_id)
{
    player()->char_id = f_char_id;
}

void AOClient::setFirstPerson(bool f_first_person)
{
    player()->first_person = f_first_person;
}

bool AOClient::isGlobalEnabled() const
{
    return m_session->global_enabled;
}

void AOClient::setGlobalEnabled(bool f_global_enabled)
{
    m_session->global_enabled = f_global_enabled;
}

QList<bool> AOClient::casingPreferences() const
{
    return m_session->casing_preferences;
}

QList<int> AOClient::charCurseList() const
{
    return m_session->charcurse_list;
}

void AOClient::addCharCurse(int f_char_id)
{
    m_session->charcurse_list.append(f_char_id);
}

void AOClient::clearCharCurse()
{
    m_session->charcurse_list.clear();
}

void AOClient::closeSocket()
{
    m_session->transport->close();
}

bool AOClient::isIcMessageAllowed() const
{
    return m_server->areaById(areaId())->isMessageAllowed() && m_server->isMessageAllowed();
}

bool AOClient::canActInArea()
{
    AreaData *l_area = m_server->areaById(areaId());
    return !(l_area->lockStatus() == AreaData::LockStatus::SPECTATABLE && !l_area->invited().contains(clientId()) && !canPerform(ACLRole::BYPASS_LOCKS));
}

bool AOClient::isIniswapAllowed() const
{
    return m_server->areaById(areaId())->isIniswapAllowed();
}

bool AOClient::isBlankpostingAllowed() const
{
    return m_server->areaById(areaId())->isBlankpostingAllowed();
}

bool AOClient::isShoutAllowed() const
{
    return m_server->areaById(areaId())->isShoutAllowed();
}

bool AOClient::isShownameAllowed() const
{
    return m_server->areaById(areaId())->isShownameAllowed();
}

bool AOClient::isImmediateForced() const
{
    return m_server->areaById(areaId())->forceImmediate();
}

QString AOClient::areaSide() const
{
    return m_server->areaById(areaId())->side();
}

QStringList AOClient::lastAreaMessage() const
{
    return m_server->areaById(areaId())->lastICMessage();
}

akashi::PairInfo AOClient::resolvePair(int f_pair_id)
{
    player()->pairing_with = f_pair_id;
    akashi::PairInfo l_pair;
    AreaData *l_area = m_server->areaById(areaId());
    const QList<int> l_joined = l_area->joinedIDs();
    for (int l_client_id : l_joined) {
        AOClient *l_client = m_server->clientById(l_client_id);
        if (l_client == nullptr) {
            continue;
        }
        if (l_client->pairingWith() == player()->char_id && f_pair_id != player()->char_id && l_client->characterId() == player()->pairing_with && l_client->pos() == player()->pos) {
            l_pair.name = l_client->iniswap();
            l_pair.emote = l_client->emote();
            l_pair.offset = l_client->offset();
            l_pair.flip = l_client->flipping();
            l_pair.paired = true;
        }
    }
    return l_pair;
}

QStringList AOClient::applyTestimony(const QStringList &f_fields)
{
    QStringList l_args = f_fields;
    akashi::TestimonyRecorder *l_recorder = m_server->areaById(areaId())->testimonyRecorder();
    using State = akashi::TestimonyRecorder::State;

    QString client_name = name();
    if (client_name == "") {
        client_name = character(); // fallback in case of empty ooc name
    }

    const State l_state = l_recorder->state();
    if ((l_state == State::Recording || l_state == State::Add) && !l_args[4].isEmpty()) {
        // -1 indicates title
        if (l_recorder->statementIndex() == -1) {
            l_args[4] = "~~-- " + l_args[4] + " --";
            l_args[14] = "3";
            m_server->broadcast(akashi::Packet("RT", {"testimony1", "0"}), areaId());
        }
        addStatement(l_args);
    }
    else if (l_state == State::Update) {
        l_args = updateStatement(l_args);
    }
    else if (l_state == State::Playback) {
        std::optional<QPair<akashi::Statement, akashi::TestimonyRecorder::Playback>> l_jump;
        bool l_navigating = false;

        const QRegularExpression jump("(?<arrow>>|<)(?<int>\\d+)");
        const QRegularExpressionMatch match = jump.match(l_args[4]);

        if (l_args[4] == ">") {
            l_navigating = true;
            l_jump = l_recorder->jumpTo(l_recorder->statementIndex() + 1);
            if (l_jump) {
                sendServerMessageArea(client_name + " moved to the next statement.");
                if (l_jump->second == akashi::TestimonyRecorder::Playback::Looped) {
                    sendServerMessageArea("Last statement reached. Looping to first statement.");
                }
            }
        }
        else if (l_args[4] == "<") {
            l_navigating = true;
            l_jump = l_recorder->jumpTo(l_recorder->statementIndex() - 1);
            if (l_jump) {
                sendServerMessageArea(client_name + " moved to the previous statement.");
                if (l_jump->second == akashi::TestimonyRecorder::Playback::StayedAtFirst) {
                    sendServerMessage("First statement reached.");
                }
            }
        }
        else if (l_args[4] == "=") {
            l_navigating = true;
            l_jump = l_recorder->jumpTo(l_recorder->statementIndex());
            if (l_jump) {
                sendServerMessageArea(client_name + " repeated the current statement.");
            }
        }
        else if (match.hasMatch()) {
            l_navigating = true;
            const int jump_idx = match.captured("int").toInt();
            l_jump = l_recorder->jumpTo(jump_idx);
            if (l_jump) {
                sendServerMessageArea(client_name + " jumped to statement number " + QString::number(jump_idx) + ".");
                if (l_jump->second == akashi::TestimonyRecorder::Playback::Looped) {
                    sendServerMessageArea("Last statement reached. Looping to first statement.");
                }
                else if (l_jump->second == akashi::TestimonyRecorder::Playback::StayedAtFirst) {
                    sendServerMessage("First statement reached.");
                }
            }
        }

        if (l_jump) {
            // Replayed statements belong to nobody, so no client mistakes
            // them for its own message and clears the player's input panel.
            l_args = l_jump->first.playbackFields();
            player()->pos = l_jump->first.side();
        }
        else if (l_navigating) {
            // The old code crashed here: playback with nothing recorded.
            sendServerMessage("There is no testimony to play back.");
        }
    }

    return l_args;
}

void AOClient::addStatement(QStringList packet)
{
    if (checkTestimonySymbols(packet[4])) {
        return;
    }
    akashi::TestimonyRecorder *l_recorder = m_server->areaById(areaId())->testimonyRecorder();
    using State = akashi::TestimonyRecorder::State;
    akashi::Statement l_statement(packet);

    if (l_recorder->state() == State::Recording) {
        if (l_recorder->statementIndex() <= ConfigManager::maxStatements()) {
            l_statement.setTextColor(l_recorder->statementIndex() == -1 ? "3" : "1");
            l_recorder->record(l_statement);
        }
        else {
            sendServerMessage("Unable to add more statements. The maximum amount of statements has been reached.");
        }
    }
    else if (l_recorder->state() == State::Add) {
        // Behind the current statement; never before the title, which the
        // old code got wrong and displaced the title with.
        l_statement.setTextColor("1");
        l_recorder->insert(qMax(l_recorder->statementIndex(), 0) + 1, l_statement);
        l_recorder->setState(State::Playback);
    }
}

QStringList AOClient::updateStatement(QStringList packet)
{
    if (checkTestimonySymbols(packet[4])) {
        return packet;
    }
    akashi::TestimonyRecorder *l_recorder = m_server->areaById(areaId())->testimonyRecorder();
    l_recorder->setState(akashi::TestimonyRecorder::State::Playback);
    const int l_index = l_recorder->statementIndex();
    if (l_index <= 0 || !l_recorder->statementAt(l_index)) {
        sendServerMessage("Unable to update an empty statement. Please use /addtestimony.");
        return packet;
    }
    akashi::Statement l_statement(packet);
    l_statement.setTextColor("1");
    l_recorder->replace(l_index, l_statement);
    sendServerMessage("Updated current statement.");
    return l_statement.icFields();
}

bool AOClient::checkTestimonySymbols(const QString &message)
{
    if (message.contains('>') || message.contains('<')) {
        sendServerMessage("Unable to add statements containing '>' or '<'.");
        return true;
    }
    return false;
}

void AOClient::broadcastIc(const QStringList &f_fields, int f_evidence_index)
{
    QStringList l_fields = f_fields;
    if (player()->pos != "") {
        l_fields[5] = player()->pos;
    }
    AreaData *l_area = m_server->areaById(areaId());

    // Presenting evidence in a hidden-CM area reveals it to everyone, and each
    // client gets the index as it appears in its own filtered list.
    int l_real_index = -1;
    bool l_evidence_presented = false;
    if (f_evidence_index > 0 && l_area->eviMod() == AreaData::EvidenceMod::HIDDEN_CM) {
        l_real_index = l_area->evidenceIndexByVisibleIndex(f_evidence_index, player()->pos, canPerform(ACLRole::CM));
        if (l_real_index >= 0) {
            l_area->setEvidenceOwnerToAll(l_real_index);
            sendEvidenceList(l_area);
            l_evidence_presented = true;
        }
    }

    const akashi::Packet l_classic("MS", l_fields);
    const QVector<AOClient *> l_clients = m_server->clients();
    for (AOClient *l_client : l_clients) {
        if (l_client->areaId() != areaId()) {
            continue;
        }
        QStringList l_client_fields = l_fields;
        if (l_evidence_presented) {
            l_client_fields[11] = QString::number(l_area->visibleIndexByEvidenceIndex(l_real_index, l_client->pos(), l_client->canPerform(ACLRole::CM)));
        }
        if (l_evidence_presented) {
            l_client->sendPacket(akashi::Packet("MS", l_client_fields));
        }
        else {
            l_client->sendPacket(l_classic);
        }
    }

    Q_EMIT logIC(l_area->name(), m_session->ipid, name(), QString::number(clientId()), (character() + " " + characterName()), player()->last_message);
    l_area->updateLastICMessage(l_fields);

    l_area->startMessageFloodguard(ConfigManager::messageFloodguard());
    m_server->startMessageFloodguard(ConfigManager::globalMessageFloodguard());
}

bool AOClient::hasSong(const QString &f_name) const
{
    return m_server->areaById(areaId())->jukebox()->hasSong(f_name);
}

bool AOClient::isDjBlocked() const
{
    return m_session->hasSanction(akashi::Sanction::DjBlock);
}

bool AOClient::isMusicAllowed() const
{
    return m_server->areaById(areaId())->isMusicAllowed() || canPerform(ACLRole::CM);
}

bool AOClient::isJukeboxEnabled() const
{
    return m_server->areaById(areaId())->isJukeboxEnabled();
}

QString AOClient::queueJukeboxSong(const QString &f_song)
{
    return m_server->areaById(areaId())->jukebox()->queueSong(clientId(), f_song);
}

QString AOClient::resolveSongAlias(const QString &f_song)
{
    const auto l_info = m_server->areaById(areaId())->jukebox()->songInfo(f_song);
    return l_info ? l_info->real_name : f_song;
}

void AOClient::recordMusicChange(const QString &f_song)
{
    AreaData *l_area = m_server->areaById(areaId());
    Q_EMIT logMusic((character() + " " + characterName()), name(), m_session->ipid, l_area->name(), f_song);

    // An empty showname would show as "played by ." in /currentmusic.
    if (characterName().isEmpty()) {
        l_area->changeMusic(character(), f_song);
        return;
    }
    l_area->changeMusic(characterName(), f_song);
}

bool AOClient::isWtceBlocked() const
{
    return m_session->hasSanction(akashi::Sanction::WtceBlock);
}

bool AOClient::isWtceAllowed() const
{
    return m_server->areaById(areaId())->isWtceAllowed();
}

bool AOClient::startWtceCooldown()
{
    const qint64 l_now = QDateTime::currentDateTime().toSecsSinceEpoch();
    if (l_now - m_session->last_wtce_time <= 5) {
        return false;
    }
    m_session->last_wtce_time = l_now;
    return true;
}

void AOClient::logJudgeAction(const QString &f_action)
{
    updateJudgeLog(m_server->areaById(areaId()), this, f_action);
}

void AOClient::addEvidence(const QString &f_name, const QString &f_description, const QString &f_image)
{
    AreaData::Evidence l_evidence;
    l_evidence.name = f_name;
    l_evidence.description = f_description;
    l_evidence.image = f_image;
    AreaData *l_area = m_server->areaById(areaId());
    l_area->appendEvidence(l_evidence);
    sendEvidenceList(l_area);
}

void AOClient::broadcastArea(const akashi::Packet &f_packet)
{
    m_server->broadcast(f_packet, areaId());
}

void AOClient::setPenalty(int f_bar, int f_value)
{
    AreaData *l_area = m_server->areaById(areaId());
    if (f_bar == 1) {
        l_area->changeHP(AreaData::Side::DEFENCE, f_value);
    }
    else if (f_bar == 2) {
        l_area->changeHP(AreaData::Side::PROSECUTOR, f_value);
    }
}

int AOClient::penalty(int f_bar) const
{
    AreaData *l_area = m_server->areaById(areaId());
    return f_bar == 1 ? l_area->defHP() : l_area->proHP();
}

void AOClient::broadcastCaseAlert(const QList<bool> &f_needs, const akashi::Packet &f_packet)
{
    const QSet<bool> l_needs_set(f_needs.begin(), f_needs.end());
    const QVector<AOClient *> l_clients = m_server->clients();
    for (AOClient *l_client : l_clients) {
        const QList<bool> l_preferences = l_client->casingPreferences();
        QSet<bool> l_matches(l_preferences.begin(), l_preferences.end());
        l_matches.intersect(l_needs_set);
        if (!l_matches.isEmpty()) {
            l_client->sendPacket(f_packet);
        }
    }
}

void AOClient::setCharacterPassword(const QString &f_password)
{
    m_session->password = f_password;
}

bool AOClient::canPerform(const QString &f_permission) const
{
    return canPerform(ACLRole::PERMISSION_CAPTIONS.key(f_permission, ACLRole::NONE));
}

QString AOClient::areaName() const
{
    return m_server->areaById(areaId())->name();
}

std::optional<QString> AOClient::playerName(int f_client_id) const
{
    AOClient *l_client = m_server->clientById(f_client_id);
    if (l_client == nullptr) {
        return std::nullopt;
    }
    return l_client->name();
}

void AOClient::broadcastModerators(const akashi::Packet &f_packet)
{
    const QVector<AOClient *> l_clients = m_server->clients();
    for (AOClient *l_client : l_clients) {
        if (l_client->isAuthenticated()) {
            l_client->sendPacket(f_packet);
        }
    }
}

void AOClient::recordModcall()
{
    Q_EMIT logModcall(m_server->areaById(areaId())->name(), m_session->ipid, name(), QString::number(clientId()), (character() + " " + characterName()));
}

void AOClient::requestModcallWebhook(const QString &f_reason)
{
    QString l_name = name();
    if (l_name.isEmpty()) {
        l_name = character();
    }
    const QString l_area_name = m_server->areaById(areaId())->name();
    Q_EMIT m_server->modcallWebhookRequest(l_name, l_area_name, QString::number(clientId()), f_reason, m_server->areaBuffer(l_area_name));
}

void AOClient::kickPlayer(int f_client_id, const QString &f_reason)
{
    AOClient *l_target = m_server->clientById(f_client_id);
    if (l_target == nullptr) {
        return;
    }
    QString l_moderator_name = "Moderator";
    if (ConfigManager::authType() == DataTypes::AuthType::ADVANCED) {
        l_moderator_name = m_session->moderator_name;
    }

    const QList<AOClient *> l_clients = m_server->clientsByIpid(l_target->ipid());
    for (AOClient *l_client : l_clients) {
        l_client->sendPacket("KK", {f_reason});
        l_client->closeSocket();
    }

    Q_EMIT logKick(l_moderator_name, l_target->ipid(), f_reason);
    sendServerMessage("Kicked " + QString::number(l_clients.size()) + " client(s) with ipid " + l_target->ipid() + " for reason: " + f_reason);
}

void AOClient::banPlayer(int f_client_id, int f_duration, const QString &f_reason)
{
    AOClient *l_target = m_server->clientById(f_client_id);
    if (l_target == nullptr) {
        return;
    }
    QString l_moderator_name = "Moderator";
    if (ConfigManager::authType() == DataTypes::AuthType::ADVANCED) {
        l_moderator_name = m_session->moderator_name;
    }

    DBManager::BanInfo l_ban;
    l_ban.ip = l_target->remoteIp();
    l_ban.ipid = l_target->ipid();
    l_ban.moderator = l_moderator_name;
    l_ban.reason = f_reason;
    l_ban.time = QDateTime::currentDateTime().toSecsSinceEpoch();

    QString l_timestamp;
    if (f_duration == -1) {
        l_ban.duration = -2;
        l_timestamp = "permanently";
    }
    else {
        l_ban.duration = f_duration * 60;
        l_timestamp = QDateTime::fromSecsSinceEpoch(l_ban.time).addSecs(l_ban.duration).toString("MM/dd/yyyy, hh:mm");
    }

    const QList<AOClient *> l_clients = m_server->clientsByIpid(l_target->ipid());
    for (AOClient *l_client : l_clients) {
        l_ban.hdid = l_client->hwid();
        m_server->databaseManager()->addBan(l_ban);
        l_client->sendPacket("KB", {f_reason});
        l_client->closeSocket();
    }

    Q_EMIT logBan(l_moderator_name, l_target->ipid(), l_timestamp, f_reason);
    sendServerMessage("Banned " + QString::number(l_clients.size()) + " client(s) with ipid " + l_target->ipid() + " for reason: " + f_reason);

    const int l_ban_id = m_server->databaseManager()->banId(l_ban.ip);
    if (ConfigManager::discordBanWebhookEnabled()) {
        Q_EMIT m_server->banWebhookRequest(l_ban.ipid, l_ban.moderator, l_timestamp, l_ban.reason, l_ban_id);
    }
}
