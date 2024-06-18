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
#include "include/music_manager.h"
#include "include/packet/packet_factory.h"
#include "include/server.h"

// This file is for commands under the music category in aoclient.h
// Be sure to register the command in the header before adding it here!

void AOClient::cmdPlay(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    if (m_is_dj_blocked) {
        sendServerMessage("You are blocked from changing the music.");
        return;
    }
    AreaData *l_area = server->getAreaById(currentArea());
    const ACLRole l_role = server->getACLRolesHandler()->getRoleById(m_acl_role_id);
    if (!l_area->owners().contains(clientId()) && !l_area->isPlayEnabled() && !l_role.checkPermission(ACLRole::CM)) { // Make sure we have permission to play music
        sendServerMessage("Free music play is disabled in this area.");
        return;
    }
    QString l_song = argv.join(" ");
    if (m_showname.isEmpty()) {
        l_area->changeMusic(currentCharacter(), l_song);
    }
    else {
        l_area->changeMusic(m_showname, l_song);
    }
    AOPacket *music_change = PacketFactory::createPacket("MC", {l_song, QString::number(server->getCharID(currentCharacter())), m_showname, "1", "0"});
    server->broadcast(music_change, currentArea());
}

void AOClient::cmdPlayAmbience(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    if (m_is_dj_blocked) {
        sendServerMessage("You are blocked from changing the ambience.");
        return;
    }
    AreaData *l_area = server->getAreaById(currentArea());
    if (!l_area->owners().contains(clientId()) && !l_area->isPlayEnabled()) { // Make sure we have permission to play music
        sendServerMessage("Free ambience play is disabled in this area.");
        return;
    }
    QString l_song = argv.join(" ");
    l_area->changeAmbience(l_song);
    AOPacket *music_change = PacketFactory::createPacket("MC", {l_song, "-1", m_showname, "1", "1"});
    server->broadcast(music_change, currentArea());
}

void AOClient::cmdCurrentMusic(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    AreaData *l_area = server->getAreaById(currentArea());
    if (!l_area->currentMusic().isEmpty() && !l_area->currentMusic().contains("~stop.mp3")) // dummy track for stopping music
        sendServerMessage("The current song is " + l_area->currentMusic() + " played by " + l_area->musicPlayerBy());
    else
        sendServerMessage("There is no music playing.");
}

void AOClient::cmdBlockDj(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    bool conv_ok = false;
    int l_uid = argv[0].toInt(&conv_ok);
    if (!conv_ok) {
        sendServerMessage("Invalid user ID.");
        return;
    }

    AOClient *l_target = server->getClientByID(l_uid);

    if (l_target == nullptr) {
        sendServerMessage("No client with that ID found.");
        return;
    }

    if (l_target->m_is_dj_blocked)
        sendServerMessage("That player is already DJ blocked!");
    else {
        sendServerMessage("DJ blocked player.");
        l_target->sendServerMessage("You were blocked from changing the music by a moderator. " + getReprimand());
    }
    l_target->m_is_dj_blocked = true;
}

void AOClient::cmdUnBlockDj(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    bool conv_ok = false;
    int l_uid = argv[0].toInt(&conv_ok);
    if (!conv_ok) {
        sendServerMessage("Invalid user ID.");
        return;
    }

    AOClient *l_target = server->getClientByID(l_uid);

    if (l_target == nullptr) {
        sendServerMessage("No client with that ID found.");
        return;
    }

    if (!l_target->m_is_dj_blocked)
        sendServerMessage("That player is not DJ blocked!");
    else {
        sendServerMessage("DJ permissions restored to player.");
        l_target->sendServerMessage("A moderator restored your music permissions. " + getReprimand(true));
    }
    l_target->m_is_dj_blocked = false;
}

void AOClient::cmdToggleMusic(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    AreaData *l_area = server->getAreaById(currentArea());
    l_area->toggleMusic();
    QString l_state = l_area->isMusicAllowed() ? "allowed." : "disallowed.";
    sendServerMessage("Music in this area is now " + l_state);
}

void AOClient::cmdToggleJukebox(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    AreaData *l_area = server->getAreaById(currentArea());
    l_area->toggleJukebox();
    QString l_state = l_area->isjukeboxEnabled() ? "enabled." : "disabled.";
    sendServerMessageArea("The jukebox in this area has been " + l_state);
}

void AOClient::cmdAddSong(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    // This needs some explanation.
    // Akashi has no concept of argument count,so any space is interpreted as a new element
    // in the QStringList. This works fine until someone enters something with a space.
    // Since we can't preencode those elements, we join all as a string and use a delimiter
    // that does not exist in file and URL paths. I decided on the ol' reliable ','.
    QString l_argv_string = argv.join(" ");
    QStringList l_argv = l_argv_string.split(",");

    bool l_success = false;
    if (l_argv.size() == 1) {
        QString l_song_name = l_argv.value(0);
        l_success = m_music_manager->addCustomSong(l_song_name, l_song_name, 0, currentArea());
    }

    if (l_argv.size() == 2) {
        QString l_song_name = l_argv.value(0);
        QString l_true_name = l_argv.value(1);
        l_success = m_music_manager->addCustomSong(l_song_name, l_true_name, 0, currentArea());
    }

    if (l_argv.size() == 3) {
        QString l_song_name = l_argv.value(0);
        QString l_true_name = l_argv.value(1);
        bool ok;
        int l_song_duration = l_argv.value(2).toInt(&ok);
        if (!ok)
            l_song_duration = 0;
        l_success = m_music_manager->addCustomSong(l_song_name, l_true_name, l_song_duration, currentArea());
    }

    if (l_argv.size() >= 4) {
        sendServerMessage("Too many arguments. Addition of song has failed.");
        return;
    }

    QString l_message = l_success ? "succeeded." : "failed.";
    sendServerMessage("The addition of the song has " + l_message);
}

void AOClient::cmdAddCategory(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    bool l_success = m_music_manager->addCustomCategory(argv.join(" "), currentArea());
    QString l_message = l_success ? "succeeded." : "failed.";
    sendServerMessage("The addition of the category has " + l_message);
}

void AOClient::cmdRemoveCategorySong(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    bool l_success = m_music_manager->removeCategorySong(argv.join(" "), currentArea());
    QString l_message = l_success ? "succeeded." : "failed.";
    sendServerMessage("The removal of the entry has " + l_message);
}

void AOClient::cmdToggleRootlist(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);
    bool l_status = m_music_manager->toggleRootEnabled(currentArea());
    QString l_message = (l_status) ? "enabled." : "disabled.";
    sendServerMessage("Global musiclist has been " + l_message);
}

void AOClient::cmdClearCustom(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);
    m_music_manager->clearCustomList(currentArea());
    sendServerMessage("Custom songs have been cleared.");
}

void AOClient::cmdJukeboxSkip(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QString l_name = currentCharacter();
    if (!m_showname.isEmpty()) {
        l_name = m_showname;
    }

    AreaData *l_area = server->getAreaById(currentArea());

    if (l_area->isjukeboxEnabled()) {
        if (l_area->getJukeboxQueueSize() >= 1) {
            l_area->switchJukeboxSong();
            sendServerMessageArea(l_name + " has forced a skip. Playing the next available song.");
            return;
        }
        sendServerMessage("Unable to skip song. Jukebox is currently empty.");
        return;
    }
    sendServerMessage("Unable to skip song. The jukebox is not running.");
}
