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

// This file is for commands under the music category in aoclient.h
// Be sure to register the command in the header before adding it here!

void AOClient::cmdPlay(int argc, QStringList argv)
{
    if (is_dj_blocked) {
        sendServerMessage("You are blocked from changing the music.");
        return;
    }
    AreaData* area = server->areas[current_area];
    QString song = argv.join(" ");
    area->currentMusic() = song;
    area->musicPlayerBy() = showname;
    AOPacket music_change("MC", {song, QString::number(server->getCharID(current_char)), showname, "1", "0"});
    server->broadcast(music_change, current_area);
}

void AOClient::cmdCurrentMusic(int argc, QStringList argv)
{
    AreaData* area = server->areas[current_area];
    if (area->currentMusic() != "" && area->currentMusic() != "~stop.mp3") // dummy track for stopping music
        sendServerMessage("The current song is " + area->currentMusic() + " played by " + area->musicPlayerBy());
    else
        sendServerMessage("There is no music playing.");
}

void AOClient::cmdBlockDj(int argc, QStringList argv)
{
    bool conv_ok = false;
    int uid = argv[0].toInt(&conv_ok);
    if (!conv_ok) {
        sendServerMessage("Invalid user ID.");
        return;
    }

    AOClient* target = server->getClientByID(uid);

    if (target->is_dj_blocked)
        sendServerMessage("That player is already DJ blocked!");
    else {
        sendServerMessage("DJ blocked player.");
        target->sendServerMessage("You were blocked from changing the music by a moderator. " + getReprimand());
    }
    target->is_dj_blocked = true;
}

void AOClient::cmdUnBlockDj(int argc, QStringList argv)
{
    bool conv_ok = false;
    int uid = argv[0].toInt(&conv_ok);
    if (!conv_ok) {
        sendServerMessage("Invalid user ID.");
        return;
    }

    AOClient* target = server->getClientByID(uid);

    if (!target->is_dj_blocked)
        sendServerMessage("That player is not DJ blocked!");
    else {
        sendServerMessage("DJ permissions restored to player.");
        target->sendServerMessage("A moderator restored your music permissions. " + getReprimand(true));
    }
    target->is_dj_blocked = false;
}

void AOClient::cmdToggleMusic(int argc, QStringList argv)
{
    AreaData* area = server->areas[current_area];
    area->toggleMusic() = !area->toggleMusic();
    QString state = area->toggleMusic() ? "allowed." : "disallowed.";
    sendServerMessage("Music in this area is now " + state);
}
