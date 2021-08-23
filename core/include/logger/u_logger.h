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

class ULogger : public QObject
{
    Q_OBJECT
public:
    ULogger(QObject* parent = nullptr);
    virtual ~ULogger();

public slots:

    void logIC(MessageLog f_log);
    void logOOC(MessageLog f_log);
    void logLogin(LoginLog f_log);
    void logCMD(CommandLog f_log);
    void logKick(ModerativeLog f_log);
    void logBan(ModerativeLog f_log);
    void logConnectionAttempt(ConnectionLog f_log);

private:

    void updateAreaBuffer(const QString& f_area, const QString& f_entry);

    /**
     * @brief QMap of all available area buffers.
     *
     * @details This QMap uses the area name as the index key to access its respective buffer.
     */
    QMap<QString, QQueue<QString>> m_bufferMap;
};

#endif //U_LOGGER_H
