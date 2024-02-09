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

#include "include/config_manager.h"
#include "include/logger/writer_full.h"
#include "include/logger/writer_modcall.h"
#include <QDateTime>
#include <QMap>
#include <QObject>
#include <QQueue>

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
    ULogger(QObject *parent = nullptr);

    /**
     * @brief Deconstructor of the universal logger. Deletes its writer before deconstruction.
     */
    virtual ~ULogger();

    /**
     * @brief Returns the buffer of a respective area. Primarily used by the Discord Webhook.
     * @param Name of the area which buffer is requested.
     */
    QQueue<QString> buffer(const QString &f_areaName);

  public slots:

    /**
     * @brief Adds an IC log entry to the area buffer and writes it to the respective log format.
     */
    void logIC(const QString &f_char_name, const QString &f_ooc_name, const QString &f_ipid,
               const QString &f_area_name, const QString &f_message);

    /**
     * @brief Adds an OOC log entry to the area buffer and writes it to the respective log format.
     */
    void logOOC(const QString &f_char_Name, const QString &f_ooc_name, const QString &f_ipid,
                const QString &f_area_name, const QString &f_message);

    /**
     * @brief Adds an login attempt to the area buffer and writes it to the respective log format.
     */
    void logLogin(const QString &f_char_name, const QString &f_ooc_name, const QString &f_moderator_name,
                  const QString &f_ipid, const QString &f_area_name, const bool &f_success);

    /**
     * @brief Adds a command usage to the area buffer and writes it to the respective log format.
     */
    void logCMD(const QString &f_char_name, const QString &f_ipid, const QString &f_ooc_name, const QString &f_command,
                const QStringList &f_args, const QString &f_area_name);

    /**
     * @brief Adds a player kick to the area buffer and writes it to the respective log format.
     */
    void logKick(const QString &f_moderator, const QString &f_target_ipid);

    /**
     * @brief Adds a player ban to the area buffer and writes it to the respective log format.
     */
    void logBan(const QString &f_moderator, const QString &f_target_ipid, const QString &f_duration);

    /**
     * @brief Adds a modcall event to the area buffer, also triggers modcall writing.
     */
    void logModcall(const QString &f_char_name, const QString &f_ipid, const QString &f_ooc_name, const QString &f_area_name);

    /**
     * @brief Logs any connection attempt to the server, wether sucessful or not.
     */
    void logConnectionAttempt(const QString &f_ip_address, const QString &f_ipid, const QString &f_hwid);

    /**
     * @brief Loads template strings for the logger.
     */
    void loadLogtext();

  private:
    /**
     * @brief Updates the area buffer with a new entry, moving old ones if the buffer exceesed the maximum size.
     * @param Name of the area which buffer is modified.
     * @param Formatted QString to be added into the buffer.
     */
    void updateAreaBuffer(const QString &f_areaName, const QString &f_log_entry);

    /**
     * @brief QMap of all available area buffers.
     *
     * @details This QMap uses the area name as the index key to access its respective buffer.
     */
    QMap<QString, QQueue<QString>> m_bufferMap;

    /**
     * @brief Pointer to modcall writer. Handles QQueue delogging into area specific file.
     */
    WriterModcall *writerModcall;

    /**
     * @brief Pointer to full writer. Handles single messages in one file.
     */
    WriterFull *writerFull;

    /**
     * @brief Table that contains template strings for text-based logger format.
     * @details To keep ConfigManager cleaner the logstrings are loaded from an inifile by name.
     *          This has the problem of lacking defaults that work for all when the file is missing.
     *          This QMap contains all default values and overwrites them on logger construction.
     */
    QHash<QString, QString> m_logtext{
        {"ic", "[%1][%5][IC][%2(%3)][%4]%6"},
        {"ooc", "[%1][%5][OOC][%2(%3)][%4]%6"},
        {"login", "[%1][LOGIN][%2][%3][%4(%5)]"},
        {"cmdlogin", "[%1][%2][LOGIN][%5][%3(%4)]"},
        {"cmdrootpass", "[%1][%2][ROOTPASS][%5][%3(%4)]"},
        {"cmdadduser", "[%1][%2][USERADD][%6][%3(%4)]%5"},
        {"cmd", "[%1][%2][CMD][%7][%3(%4)]/%5 %6"},
        {"kick", "[%1][%2][KICK][%3]"},
        {"ban", "[%1][%2][BAN][%3][%4]"},
        {"modcall", "[%1][%2][MODCALL][%5][%3(%4)]"},
        {"connect", "[%1][CONNECT][%2][%3][%4]"}};
};

#endif // U_LOGGER_H
