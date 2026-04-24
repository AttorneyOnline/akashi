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
#include "player_directory.h"

void PlayerDirectory::setCapacity(int f_capacity)
{
    clear();
    for (int i = f_capacity - 1; i >= 0; i--) {
        m_free_ids.push(i);
    }
}

bool PlayerDirectory::isFull() const
{
    return m_free_ids.isEmpty();
}

int PlayerDirectory::takeId()
{
    if (m_free_ids.isEmpty()) {
        return -1;
    }
    return m_free_ids.pop();
}

void PlayerDirectory::returnId(int f_id)
{
    m_free_ids.push(f_id);
}

void PlayerDirectory::addClient(int f_id, AOClient *f_client)
{
    Q_ASSERT(!m_clients_by_id.contains(f_id));
    m_clients_by_id.insert(f_id, f_client);
    m_clients.append(f_client);
}

void PlayerDirectory::removeClient(int f_id)
{
    AOClient *l_client = m_clients_by_id.take(f_id);
    if (!l_client) {
        return;
    }
    m_clients.removeAll(l_client);
    m_free_ids.push(f_id);
}

AOClient *PlayerDirectory::clientById(int f_id) const
{
    return m_clients_by_id.value(f_id);
}

QVector<AOClient *> PlayerDirectory::clients() const
{
    return m_clients;
}

int PlayerDirectory::clientCount() const
{
    return m_clients.size();
}

void PlayerDirectory::clear()
{
    m_clients.clear();
    m_clients_by_id.clear();
    m_free_ids.clear();
}
