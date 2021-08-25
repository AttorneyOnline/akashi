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
#include <QDateTime>
#include "include/config_manager.h"
#include "include/logger/writer_full.h"
#include "include/logger/writer_modcall.h"
#include "include/logger/writer_sql.h"

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
     */
    void logIC(const QString& f_charName, const QString& f_oocName, const QString& f_ipid,
               const QString& f_areaName, const QString &f_message);

    /**
     * @brief Adds an OOC log entry to the area buffer and writes it to the respective log format.
     */
    void logOOC(const QString& f_charName, const QString& f_oocName, const QString& f_ipid,
                const QString& f_areaName, const QString& f_message);

    /**
     * @brief Adds an login attempt to the area buffer and writes it to the respective log format.
     */
    void logLogin(const QString& f_charName, const QString& f_oocName, const QString& f_moderatorName,
                  const QString& f_ipid, const QString &f_areaName, const bool& f_success);

    /**
     * @brief Adds a command usage to the area buffer and writes it to the respective log format.
     */
    void logCMD(const QString& f_charName, const QString &f_ipid, const QString& f_oocName, const QString f_command,
                const QStringList f_args, const QString f_areaName);

    /**
     * @brief Adds a player kick to the area buffer and writes it to the respective log format.
     */
    void logKick(const QString& f_moderator, const QString& f_targetIPID, const QString& f_targetName, const QString f_targetOOCName);

    /**
     * @brief Adds a player ban to the area buffer and writes it to the respective log format.
     */
    void logBan(const QString& f_moderator, const QString& f_targetIPID, const QString& f_targetName, const QString f_targetOOCName,
                const QString &f_duration);

    /**
     * @brief Adds a modcall event to the area buffer, also triggers modcall writing.
     */
    void logModcall(const QString& f_charName, const QString &f_ipid, const QString& f_oocName, const QString& f_areaName);

    /**
     * @brief Logs any connection attempt to the server, wether sucessful or not.
     */
    void logConnectionAttempt(const QString &f_ip_address, const QString &f_ipid);

private:

    /**
     * @brief Updates the area buffer with a new entry, moving old ones if the buffer exceesed the maximum size.
     * @param Name of the area which buffer is modified.
     * @param Formatted QString to be added into the buffer.
     */
    void updateAreaBuffer(const QString& f_areaName, const QString& f_logEntry);

    /**
     * @brief Returns the buffer of a respective area. Primarily used by the Discord Webhook.
     * @param Name of the area which buffer is requested.
     */
    QQueue<QString> buffer(const QString &f_areaName);

    /**
     * @brief QMap of all available area buffers.
     *
     * @details This QMap uses the area name as the index key to access its respective buffer.
     */
    QMap<QString, QQueue<QString>> m_bufferMap;

    /**
     * @brief Pointer to modcall writer. Handles QQueue delogging into area specific file.
     */
    WriterModcall* writerModcall;

    /**
     * @brief Pointer to full writer. Handles single messages in one file.
     */
    WriterFull* writerFull;

    /**
     * @brief Pointer to SQL writer. Handles execution of log SQL queries.
     */
    WriterSQL* writerSQL;
};

#endif //U_LOGGER_H
