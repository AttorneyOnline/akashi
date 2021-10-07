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
#ifndef SERVER_DATA_H
#define SERVER_DATA_H

#include <QVector>
#include <QStringList>
#include <QTimer>
#include "include/db_manager.h"
//Cringe forward declarations
class AOClient;
class AreaData;
class Server;

class ServerData
{
public:
    /**
     * @brief The collection of all currently connected clients.
     */
    QVector<AOClient*> m_clients;

    /**
     * @brief The overall player count in the server.
     */
    int m_player_count = 0;

    /**
     * @brief The characters available on the server to use.
     */
    QStringList m_characters;

    /**
     * @brief The areas on the server.
     */
    QVector<AreaData*> m_areas;

    /**
     * @brief The names of the areas on the server.
     *
     * @details Equivalent to iterating over #areas and getting the area names individually, but grouped together
     * here for faster access.
     */
    QStringList m_area_names;

    /**
     * @brief The available songs on the server.
     *
     * @details Does **not** include the area names, the actual music list packet should be constructed from
     * #area_names and this combined.
     */
    QStringList m_music_list;

    /**
     * @brief The backgrounds on the server that may be used in areas.
     */
    QStringList m_backgrounds;

    /**
     * @brief Returns the character's character ID (= their index in the character list).
     *
     * @param char_name The 'internal' name for the character whose character ID to look up. This is equivalent to
     * the name of the directory of the character.
     *
     * @return The character ID if a character with that name exists in the character selection list, `-1` if not.
     */
    int getCharID(QString char_name);

    /**
     * @brief Gets a pointer to a client by user ID.
     *
     * @param id The user ID to look for.
     *
     * @return A pointer to the client if found, a nullpointer if not.
     */
    AOClient* getClientByID(int id);

    /**
     * @brief Gets a pointer to a client by IPID.
     *
     * @param ipid The IPID to look for.
     *
     * @return A pointer to the client if found, a nullpointer if not.
     *
     * @see Server::getClientsByIpid() to get all clients ran by the same user.
     */
    AOClient* getClient(QString ipid);

    /**
     * @brief Gets a list of pointers to all clients with the given IPID.
     *
     * @param ipid The IPID to look for.
     *
     * @return A list of clients whose IPID match. List may be empty.
     */
    QList<AOClient*> getClientsByIpid(QString ipid);

    QStringList getCursedCharsTaken(AOClient* client, QStringList chars_taken);

    /**
     * @brief Updates which characters are taken in the given area, and sends out an update packet to
     * all clients present the area.
     *
     * @param area The area in which to update the list of characters.
     */
    void updateCharsTaken(AreaData* area);

    /**
     * @brief The database manager on the server, used to store users' bans and authorisation details.
     */
    DBManager* db_manager;

    /**
     * @brief The server-wide global timer.
     */
    QTimer* timer;

    /**
     * @brief Timer until the next IC message can be sent.
     */
    QTimer next_message_timer;

    /**
     * @brief If false, IC messages will be rejected.
     */
    bool can_send_ic_messages = true;

};

#endif //SERVER_DATA_H
