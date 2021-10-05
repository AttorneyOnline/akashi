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
#include "include/server_data.h"
#include "include/aoclient.h"

QList<AOClient*> ServerData::getClientsByIpid(QString f_ipid)
{
    QList<AOClient*> return_clients;
    for (AOClient* client : qAsConst(m_clients)) {
        if (client->getIpid() == f_ipid)
            return_clients.append(client);
    }
    return return_clients;
}

AOClient* ServerData::getClientByID(int f_id)
{
    for (AOClient* client : qAsConst(m_clients)) {
        if (client->m_id == f_id)
            return client;
    }
    return nullptr;
}

void ServerData::updateCharsTaken(AreaData* area)
{
    QStringList chars_taken;
    for (const QString &cur_char : qAsConst(m_characters)) {
        chars_taken.append(area->charactersTaken().contains(getCharID(cur_char))
                               ? QStringLiteral("-1")
                               : QStringLiteral("0"));
    }

    AOPacket response_cc("CharsCheck", chars_taken);

    for (AOClient* client : qAsConst(m_clients)) {
        if (client->m_current_area == area->index()){
            if (!client->m_is_charcursed)
                client->sendPacket(response_cc);
            else {
                QStringList chars_taken_cursed = getCursedCharsTaken(client, chars_taken);
                AOPacket response_cc_cursed("CharsCheck", chars_taken_cursed);
                client->sendPacket(response_cc_cursed);
            }
        }
    }
}

QStringList ServerData::getCursedCharsTaken(AOClient* client, QStringList chars_taken)
{
    QStringList chars_taken_cursed;
    for (int i = 0; i < chars_taken.length(); i++) {
        if (!client->m_charcurse_list.contains(i))
            chars_taken_cursed.append("-1");
        else
            chars_taken_cursed.append(chars_taken.value(i));
    }
    return chars_taken_cursed;
}

int ServerData::getCharID(QString char_name)
{
    for (const QString &character : qAsConst(m_characters)) {
        if (character.toLower() == char_name.toLower()) {
            return m_characters.indexOf(QRegExp(character, Qt::CaseInsensitive));
        }
    }
    return -1; // character does not exist
}
