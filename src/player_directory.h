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
#ifndef PLAYER_DIRECTORY_H
#define PLAYER_DIRECTORY_H

#include "akashi_core_export.h"

#include <QHash>
#include <QStack>
#include <QVector>

class AOClient;

// The one place that knows who is connected under which ID. It owns the ID
// pool and the client list together, so they can never disagree - the old
// design kept three separate containers in sync by hand, with "free" encoded
// as a null pointer in a pre-filled map. Lookups here are null-safe by
// construction: a free or unknown ID gives nullptr, never a stale pointer,
// and the client list never contains nulls.
class AKASHI_CORE_EXPORT PlayerDirectory
{
  public:
    // Prepares f_capacity connection slots, with IDs 0 to f_capacity - 1.
    void setCapacity(int f_capacity);

    // True when no free ID is left.
    bool isFull() const;

    // Takes a free ID out of the pool, or -1 when full. The most recently
    // returned ID is handed out first, lowest IDs first on a fresh server.
    int takeId();

    // Gives an unused ID back, for a connection rejected before it was added.
    void returnId(int f_id);

    // Files an accepted client under the ID it was given.
    void addClient(int f_id, AOClient *f_client);

    // Drops the client filed under the ID and returns the ID to the pool.
    // Unknown IDs are ignored.
    void removeClient(int f_id);

    // The client with the given ID, or nullptr when that ID is free.
    AOClient *clientById(int f_id) const;

    // Every connected client, oldest connection first.
    QVector<AOClient *> clients() const;

    int clientCount() const;

    // Forgets all clients and IDs without deleting anything; the caller
    // owns the objects. Used for the server's shutdown sequence.
    void clear();

  private:
    QVector<AOClient *> m_clients;
    QHash<int, AOClient *> m_clients_by_id;
    QStack<int> m_free_ids;
};

#endif
