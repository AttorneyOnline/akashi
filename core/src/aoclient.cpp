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
#include "include/aoclient.h"

#include "include/area_data.h"
#include "include/command_extension.h"
#include "include/config_manager.h"
#include "include/db_manager.h"
#include "include/packet/packet_factory.h"
#include "include/server.h"

const QMap<QString, AOClient::CommandInfo> AOClient::COMMANDS{
    {"login", {{ACLRole::NONE}, 0, &AOClient::cmdLogin}},
    {"getarea", {{ACLRole::NONE}, 0, &AOClient::cmdGetArea}},
    {"getareas", {{ACLRole::NONE}, 0, &AOClient::cmdGetAreas}},
    {"ban", {{ACLRole::BAN}, 3, &AOClient::cmdBan}},
    {"kick", {{ACLRole::KICK}, 2, &AOClient::cmdKick}},
    {"changeauth", {{ACLRole::SUPER}, 0, &AOClient::cmdChangeAuth}},
    {"rootpass", {{ACLRole::SUPER}, 1, &AOClient::cmdSetRootPass}},
    {"background", {{ACLRole::NONE}, 1, &AOClient::cmdSetBackground}},
    {"lock_background", {{ACLRole::BGLOCK}, 0, &AOClient::cmdBgLock}},
    {"unlock_background", {{ACLRole::BGLOCK}, 0, &AOClient::cmdBgUnlock}},
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
    {"play", {{ACLRole::CM}, 1, &AOClient::cmdPlay}},
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
    {"help", {{ACLRole::NONE}, 1, &AOClient::cmdHelp}},
    {"clearcm", {{ACLRole::KICK}, 0, &AOClient::cmdClearCM}},
    {"togglemessage", {{ACLRole::CM}, 0, &AOClient::cmdToggleAreaMessageOnJoin}},
    {"clearmessage", {{ACLRole::CM}, 0, &AOClient::cmdClearAreaMessage}},
    {"areamessage", {{ACLRole::CM}, 0, &AOClient::cmdAreaMessage}},
    {"addsong", {{ACLRole::CM}, 1, &AOClient::cmdAddSong}},
    {"addcategory", {{ACLRole::CM}, 1, &AOClient::cmdAddCategory}},
    {"removeentry", {{ACLRole::CM}, 1, &AOClient::cmdRemoveCategorySong}},
    {"toggleroot", {{ACLRole::CM}, 0, &AOClient::cmdToggleRootlist}},
    {"clearcustom", {{ACLRole::CM}, 0, &AOClient::cmdClearCustom}},
    {"toggle_wtce", {{ACLRole::CM}, 0, &AOClient::cmdToggleWtce}},
    {"toggle_shouts", {{ACLRole::CM}, 0, &AOClient::cmdToggleShouts}},
    {"kick_other", {{ACLRole::NONE}, 0, &AOClient::cmdKickOther}},
    {"jukebox_skip", {{ACLRole::CM}, 0, &AOClient::cmdJukeboxSkip}}};

void AOClient::clientDisconnected()
{
#ifdef NET_DEBUG
    qDebug() << m_remote_ip.toString() << "disconnected";
#endif
    if (m_joined) {
        server->getAreaById(m_current_area)->clientLeftArea(server->getCharID(m_current_char), m_id);
        arup(ARUPType::PLAYER_COUNT, true);
    }

    if (m_current_char != "") {
        server->updateCharsTaken(server->getAreaById(m_current_area));
    }

    bool l_updateLocks = false;

    const QVector<AreaData *> l_areas = server->getAreas();
    for (AreaData *l_area : l_areas) {
        l_updateLocks = l_updateLocks || l_area->removeOwner(m_id);
    }

    if (l_updateLocks)
        arup(ARUPType::LOCKED, true);
    arup(ARUPType::CM, true);
    emit clientSuccessfullyDisconnected(m_id);
}

void AOClient::handlePacket(AOPacket *packet)
{
#ifdef NET_DEBUG
    qDebug() << "Received packet:" << packet.getHeader() << ":" << packet.getContent() << "args length:" << packet.getContent().length();
#endif
    AreaData *l_area = server->getAreaById(m_current_area);

    if (packet->getContent().join("").size() > 16384) {
        return;
    }

    if (!checkPermission(packet->getPacketInfo().acl_permission)) {
        return;
    }

    if (packet->getPacketInfo().header != "CH" && m_joined) {
        if (m_is_afk)
            sendServerMessage("You are no longer AFK.");
        m_is_afk = false;
        m_afk_timer->start(ConfigManager::afkTimeout() * 1000);
    }

    if (packet->getContent().length() < packet->getPacketInfo().min_args) {
#ifdef NET_DEBUG
        qDebug() << "Invalid packet args length. Minimum is" << packet->getPacketInfo().min_args << "but only" << packet.getContent().length() << "were given.";
#endif
        return;
    }

    packet->handlePacket(l_area, *this);
}

void AOClient::changeArea(int new_area)
{
    if (m_current_area == new_area) {
        sendServerMessage("You are already in area " + server->getAreaName(m_current_area));
        return;
    }
    if (server->getAreaById(new_area)->lockStatus() == AreaData::LockStatus::LOCKED && !server->getAreaById(new_area)->invited().contains(m_id) && !checkPermission(ACLRole::BYPASS_LOCKS)) {
        sendServerMessage("Area " + server->getAreaName(new_area) + " is locked.");
        return;
    }

    if (m_current_char != "") {
        server->getAreaById(m_current_area)->changeCharacter(server->getCharID(m_current_char), -1);
        server->updateCharsTaken(server->getAreaById(m_current_area));
    }
    server->getAreaById(m_current_area)->clientLeftArea(m_char_id, m_id);
    bool l_character_taken = false;
    if (server->getAreaById(new_area)->charactersTaken().contains(server->getCharID(m_current_char))) {
        m_current_char = "";
        m_char_id = -1;
        l_character_taken = true;
    }
    server->getAreaById(new_area)->clientJoinedArea(m_char_id, m_id);
    m_current_area = new_area;
    arup(ARUPType::PLAYER_COUNT, true);
    sendEvidenceList(server->getAreaById(new_area));
    sendPacket("HP", {"1", QString::number(server->getAreaById(new_area)->defHP())});
    sendPacket("HP", {"2", QString::number(server->getAreaById(new_area)->proHP())});
    sendPacket("BN", {server->getAreaById(new_area)->background()});
    if (l_character_taken) {
        sendPacket("DONE");
    }
    const QList<QTimer *> l_timers = server->getAreaById(m_current_area)->timers();
    for (QTimer *l_timer : l_timers) {
        int l_timer_id = server->getAreaById(m_current_area)->timers().indexOf(l_timer) + 1;
        if (l_timer->isActive()) {
            sendPacket("TI", {QString::number(l_timer_id), "2"});
            sendPacket("TI", {QString::number(l_timer_id), "0", QString::number(QTime(0, 0).msecsTo(QTime(0, 0).addMSecs(l_timer->remainingTime())))});
        }
        else {
            sendPacket("TI", {QString::number(l_timer_id), "3"});
        }
    }
    sendServerMessage("You moved to area " + server->getAreaName(m_current_area));
    if (server->getAreaById(m_current_area)->sendAreaMessageOnJoin())
        sendServerMessage(server->getAreaById(m_current_area)->areaMessage());

    if (server->getAreaById(m_current_area)->lockStatus() == AreaData::LockStatus::SPECTATABLE)
        sendServerMessage("Area " + server->getAreaName(m_current_area) + " is spectate-only; to chat IC you will need to be invited by the CM.");
}

bool AOClient::changeCharacter(int char_id)
{
    AreaData *l_area = server->getAreaById(m_current_area);

    if (char_id >= server->getCharacterCount())
        return false;

    if (m_is_charcursed && !m_charcurse_list.contains(char_id)) {
        return false;
    }

    bool l_successfulChange = l_area->changeCharacter(server->getCharID(m_current_char), char_id);

    if (char_id < 0) {
        m_current_char = "";
        m_char_id = char_id;
        setSpectator(true);
    }

    if (l_successfulChange == true) {
        QString l_char_selected = server->getCharacterById(char_id);
        m_current_char = l_char_selected;
        m_pos = "";
        server->updateCharsTaken(l_area);
        sendPacket("PV", {QString::number(m_id), "CID", QString::number(char_id)});
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
            if (l_area->owners().isEmpty())
                l_arup_data.append("FREE");
            else {
                QStringList l_area_owners;
                const QList<int> l_owner_ids = l_area->owners();
                for (int l_owner_id : l_owner_ids) {
                    AOClient *l_owner = server->getClientByID(l_owner_id);
                    l_area_owners.append("[" + QString::number(l_owner->m_id) + "] " + l_owner->m_current_char);
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
    if (broadcast)
        server->broadcast(PacketFactory::createPacket("ARUP", l_arup_data));
    else
        sendPacket("ARUP", l_arup_data);
}

void AOClient::fullArup()
{
    arup(ARUPType::PLAYER_COUNT, false);
    arup(ARUPType::STATUS, false);
    arup(ARUPType::CM, false);
    arup(ARUPType::LOCKED, false);
}

void AOClient::sendPacket(AOPacket *packet)
{
#ifdef NET_DEBUG
    qDebug() << "Sent packet:" << packet.getHeader() << ":" << packet.getContent();
#endif
    m_socket->write(packet);
}

void AOClient::sendPacket(QString header, QStringList contents)
{
    sendPacket(PacketFactory::createPacket(header, contents));
}

void AOClient::sendPacket(QString header)
{
    sendPacket(PacketFactory::createPacket(header, {}));
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

void AOClient::sendServerMessage(QString message)
{
    sendPacket("CT", {ConfigManager::serverName(), message, "1"});
}

void AOClient::sendServerMessageArea(QString message)
{
    server->broadcast(PacketFactory::createPacket("CT", {ConfigManager::serverName(), message, "1"}), m_current_area);
}

void AOClient::sendServerBroadcast(QString message)
{
    server->broadcast(PacketFactory::createPacket("CT", {ConfigManager::serverName(), message, "1"}));
}

bool AOClient::checkPermission(ACLRole::Permission f_permission) const
{
    if (f_permission == ACLRole::NONE) {
        return true;
    }

    if ((f_permission == ACLRole::CM) && server->getAreaById(m_current_area)->owners().contains(m_id)) {
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

bool AOClient::hasJoined() const
{
    return m_joined;
}

bool AOClient::isAuthenticated() const
{
    return m_authenticated;
}

Server *AOClient::getServer() { return server; }

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
    if (!m_is_afk)
        sendServerMessage("You are now AFK.");
    m_is_afk = true;
}

AOClient::AOClient(Server *p_server, NetworkSocket *socket, QObject *parent, int user_id, MusicManager *p_manager) :
    QObject(parent),
    m_id(user_id),
    m_remote_ip(socket->peerAddress()),
    m_password(""),
    m_joined(false),
    m_current_area(0),
    m_current_char(""),
    m_socket(socket),
    server(p_server),
    is_partial(false),
    m_last_wtce_time(0),
    m_music_manager(p_manager)
{
    m_afk_timer = new QTimer;
    m_afk_timer->setSingleShot(true);
    connect(m_afk_timer, &QTimer::timeout, this, &AOClient::onAfkTimeout);
}

AOClient::~AOClient()
{
    clientDisconnected();
    m_socket->deleteLater();
}
