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
//    but WITHOUT ANY WARRANTY; without even the implied warranty of                //
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                 //
//    GNU Affero General Public License for more details.                           //
//                                                                                  //
//    You should have received a copy of the GNU Affero General Public License      //
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.        //
//////////////////////////////////////////////////////////////////////////////////////
#ifndef ICCHATPACKET_H
#define ICCHATPACKET_H

#endif // ICCHATPACKET_H

#include "include/aopacket.h"

class ICChatPacket : public AOPacket
{
public:
    ICChatPacket(AOPacket packet);

    const int MIN_FIELDS = 15;
    const int MAX_FIELDS = 30;

    bool is_valid;

    QString desk_override;
    QString preanim;
    QString character;
    QString emote;
    QString message;
    QString side;
    QString sfx_name;
    int emote_modifier;
    int char_id;
    QString sfx_delay;
    int objection_modifier;
    QString evidence;
    bool flip;
    bool realization;
    int text_color;
    QString showname;
    int other_charid;
    QString other_name;
    QString other_emote;
    int self_offset;
    int other_offset;
    bool other_flip;
    bool noninterrupting_preanim;
    bool sfx_looping;
    bool screenshake;
    QString frames_shake;
    QString frames_realization;
    QString frames_sfx;
    bool additive;
    QString effect;
};
