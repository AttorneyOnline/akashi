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
#ifndef LOGGER_H
#define LOGGER_H

#include "include/aoclient.h"
#include "include/aopacket.h"
#include "include/area_data.h"

#include <QFile>
#include <QDebug>
#include <QString>
#include <QQueue>
#include <QDateTime>

class AOClient;
class AreaData;

/**
 * @brief A class associated with an AreaData class to log various events happening inside the latter.
 */
class Logger
{
public:
    /**
     * @brief Constructs a Logger instance.
     *
     * @param p_max_length The maximum amount of entries the Logger can store at once.
     * @param p_area The area associated with the Logger from which it should log entries.
     */
    Logger(int p_max_length, AreaData* p_area) : max_length(p_max_length), area(p_area) {};

    /**
     * @brief Logs an IC message.
     *
     * @param client The client who sent the IC message.
     * @param packet The IC packet itself, used to grab the text of the IC message.
     */
    void logIC(AOClient* client, AOPacket* packet);

    /**
     * @brief Logs an OOC message.
     *
     * @param client The client who sent the OOC message.
     * @param packet The OOC packet itself, used to grab the text of the OOC message.
     */
    void logOOC(AOClient* client, AOPacket* packet);

    /**
     * @brief Logs a mod call message.
     *
     * @param client The client who sent the mod call.
     * @param packet The ZZ packet itself, used to grab the reason field of the modcall.
     */
    void logModcall(AOClient* client, AOPacket* packet);

    /**
     * @brief Logs a command called in OOC.
     *
     * @details If the command is not one of any of the 'special' ones, it defaults to logOOC().
     * The only thing that makes a command 'special' if it is handled differently in here.
     *
     * @param client The client who sent the command.
     * @param packet The OOC packet. Passed to logOOC() if the command is not 'special' (see details).
     * @param cmd The command called in the OOC -- this is the first word after the `/` character.
     * @param args The arguments interpreted for the command, every word separated by a whitespace.
     */
    void logCmd(AOClient* client, AOPacket* packet, QString cmd, QStringList args);

    /**
     * @brief Logs a login attempt.
     *
     * @param client The client that attempted to login.
     * @param success True if the client successfully authenticated as a mod.
     * @param modname If the client logged in with a modname, then this is it. Otherwise, it's `"moderator"`.
     *
     * @note Why does this exist? logCmd() already does this in part.
     */
    void logLogin(AOClient* client, bool success, QString modname);

    /**
     * @brief Appends the contents of #buffer into `config/server.log`, emptying the former.
     */
    void flush();

    /**
     *@brief Returns the current area buffer
     */
    QQueue<QString> getBuffer();

private:
    /**
     * @brief Convenience function to format entries to the acceptable standard for logging.
     *
     * @param client The client who 'caused' the source event for the entry to happen.
     * @param type The type of entry that is being built, something that uniquely identifies entries of similar being.
     * @param message Any additional information related to the entry.
     *
     * @return A formatted string representation of the entry.
     */
    QString buildEntry(AOClient* client, QString type, QString message);

    /**
     * @brief Convenience function to add an entry to #buffer.
     *
     * @details If the buffer's size is equal to #max_length, the first entry in the queue is removed,
     * and the newest entry is added to the end.
     *
     * @param entry The string representation of the entry to add.
     *
     * @pre You would probably call buildEntry() to format the entry before adding it to the buffer.
     */
    void addEntry(QString entry);

    /**
     * @brief The max amount of entries that may be contained in #buffer.
     */
    int max_length;

    /**
     * @brief Contains entries that have not yet been flushed out into a log file.
     */
    QQueue<QString> buffer;

    /**
     * @brief A pointer to the area this logger is associated with.
     *
     * @details Used for logging in what area did a given packet event happen.
     */
    AreaData* area;
};

#endif // LOGGER_H
