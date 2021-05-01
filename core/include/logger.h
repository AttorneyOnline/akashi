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

#include <QFile>
#include <QDebug>
#include <QString>
#include <QQueue>
#include <QDateTime>

/**
 * @brief A class associated with an AreaData class to log various events happening inside the latter.
 */
class Logger
{
public:
    /**
     * @brief Constructs a Logger instance.
     *
     * @param f_max_length The maximum amount of entries the Logger can store at once.
     */
    Logger(QString f_area_name, int f_max_length, const QString& f_logType_r) :
        m_areaName(f_area_name), m_maxLength(f_max_length), m_logType(f_logType_r) {};

public slots:
    /**
     * @brief Logs an IC message.
     *
     * @param f_charName_r The character name of the client who sent the IC message.
     * @param f_ipid_r The IPID of the aforementioned client.
     * @param f_message_r The text of the IC message.
     */
    void logIC(const QString& f_charName_r, const QString& f_ipid_r, const QString& f_message_r);

    /**
     * @brief Logs an OOC message.
     *
     * @param f_areaName_r The name of the area where the event happened.
     * @param f_charName_r The character name of the client who sent the OOC message.
     * @param f_ipid_r The IPID of the aforementioned client.
     * @param f_message_r The text of the OOC message.
     */
    void logOOC(const QString& f_charName_r, const QString& f_ipid_r, const QString& f_message_r);

    /**
     * @brief Logs a mod call message.
     *
     * @param f_charName_r The character name of the client who sent the mod call.
     * @param f_ipid_r The IPID of the aforementioned client.
     * @param f_modcallReason_r The reason for the modcall.
     */
    void logModcall(const QString& f_charName_r, const QString& f_ipid_r, const QString& f_modcallReason_r);

    /**
     * @brief Logs a command called in OOC.
     *
     * @details If the command is not one of any of the 'special' ones, it defaults to logOOC().
     * The only thing that makes a command 'special' if it is handled differently in here.
     *
     * @param f_charName_r The character name of the client who sent the command.
     * @param f_ipid_r The IPID of the aforementioned client.
     * @param f_oocMessage_r The text of the OOC message. Passed to logOOC() if the command is not 'special' (see details).
     */
    void logCmd(const QString& f_charName_r, const QString& f_ipid_r, const QString& f_oocMessage_r);

    /**
     * @brief Logs a login attempt.
     *
     * @param f_charName_r The character name of the client that attempted to login.
     * @param f_ipid_r The IPID of the aforementioned client.
     * @param success True if the client successfully authenticated as a mod.
     * @param f_modname_r If the client logged in with a modname, then this is it. Otherwise, it's `"moderator"`.
     */
    void logLogin(const QString& f_charName_r, const QString& f_ipid_r, bool success, const QString& f_modname_r);

    /**
     * @brief Appends the contents of #buffer into `config/server.log`, emptying the former.
     */
    void flush();

    /**
     * @brief Contains entries that have not yet been flushed out into a log file.
     */
    QQueue<QString> m_buffer;

private:
    /**
     * @brief Convenience function to add an entry to #buffer.
     *
     * @details If the buffer's size is equal to #max_length, the first entry in the queue is removed,
     * and the newest entry is added to the end.
     *
     * @param f_charName_r The character name of the client who 'caused' the source event for the entry to happen.
     * @param f_ipid_r The IPID of the aforementioned client.
     * @param f_type_r The type of entry that is being built, something that uniquely identifies entries of similar being.
     * @param f_message_r Any additional information related to the entry.
     */
    void addEntry(const QString& f_charName_r, const QString& f_ipid_r,
                  const QString& f_type_r,     const QString& f_message_r);

    /**
     * @brief The max amount of entries that may be contained in #buffer.
     */
    int m_maxLength;

    QString m_areaName;

    /**
     * @brief Determines what kind of logging happens, `"full"` or `"modcall"`.
     *
     * @details This largely influences the resulting log file's name, and in case of a `"full"` setup,
     * the in-memory buffer is auto-dumped to said file if full.
     */
    QString m_logType;
};

#endif // LOGGER_H
