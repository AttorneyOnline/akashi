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
#ifndef U_LOGGER_H
#define U_LOGGER_H

#include <QObject>
#include <QMap>
#include <QQueue>
#include "include/config_manager.h"
#include "include/logger/u_logger_datatypes.h"

/**
 * @brief The Universal Logger class to provide a common place to handle, store and write logs to file.
 */
class ULogger : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief Constructor for the universal logger. Determines which writer is initially created.
     * @param Pointer to the Server.
     */
    ULogger(QObject* parent = nullptr);

    /**
     * @brief Deconstructor of the universal logger. Deletes its writer before deconstruction.
     */
    virtual ~ULogger();

public slots:

    /**
     * @brief Adds an IC log entry to the area buffer and writes it to the respective log format.
     * @param MessageLog containing client information and the actual message.
     */
    void logIC(MessageLog f_log);

    /**
     * @brief Adds an OOC log entry to the area buffer and writes it to the respective log format.
     * @param MessageLog containing client information and the actual message.
     */
    void logOOC(MessageLog f_log);

    /**
     * @brief Adds an login attempt to the area buffer and writes it to the respective log format.
     * @param LoginLog containing info about the login attempt.
     */
    void logLogin(LoginLog f_log);

    /**
     * @brief Adds a command usage to the area buffer and writes it to the respective log format.
     * @param ComandLog containing information about the command and parameter used.
     */
    void logCMD(CommandLog f_log);

    /**
     * @brief Adds a player kick to the area buffer and writes it to the respective log format.
     * @param ModerativeLog containing information about the client kicked and who kicked them.
     */
    void logKick(ModerativeLog f_log);

    /**
     * @brief Adds a player ban to the area buffer and writes it to the respective log format.
     * @param ModerativeLog containing information about the client banned and who banned them.
     */
    void logBan(ModerativeLog f_log);

    /**
     * @brief Logs any connection attempt to the server, wether sucessful or not.
     * @param ConnectionLog containing information on who connected and if the connection was successful.
     */
    void logConnectionAttempt(ConnectionLog f_log);

private:

    /**
     * @brief Updates the area buffer with a new entry, moving old ones if the buffer exceesed the maximum size.
     * @param Name of the area which buffer is modified.
     * @param Formatted QString to be added into the buffer.
     */
    void updateAreaBuffer(const QString& f_area, const QString& f_entry);

    /**
     * @brief Returns the buffer of a respective area. Primarily used by the Discord Webhook.
     * @param Name of the area which buffer is requested.
     */
    QQueue<QString> buffer(QString f_areaName);

    /**
     * @brief QMap of all available area buffers.
     *
     * @details This QMap uses the area name as the index key to access its respective buffer.
     */
    QMap<QString, QQueue<QString>> m_bufferMap;
};

#endif //U_LOGGER_H
