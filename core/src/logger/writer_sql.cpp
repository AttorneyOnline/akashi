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
#include "include/logger/writer_sql.h"

WriterSQL::WriterSQL(QObject* parent) :
    QObject(parent), DRIVER("QSQLITE")
{
    l_dir.setPath("logs/");
    if (!l_dir.exists()) {
        l_dir.mkpath(".");
    }

    l_dir.setPath("logs/database");
    if (!l_dir.exists()) {
        l_dir.mkpath(".");
    }

    const QString db_filename = "logs/database/log.db";

    QFileInfo db_info(db_filename);
    if(!db_info.isReadable() || !db_info.isWritable())
        qCritical() << tr("Database Error: Missing permissions. Check if \"%1\" is writable.").arg(db_filename);

    log_db = QSqlDatabase::addDatabase(DRIVER);
    log_db.setDatabaseName("logs/database/log.db");

    if (!log_db.open())
        qCritical() << "Database Error:" << log_db.lastError();

    QSqlQuery create_chat_events_table("CREATE TABLE IF NOT EXISTS chat_events ('event_time' DATETIME DEFAULT CURRENT_TIMESTAMP, 'ipid' TEXT, 'room_name' TEXT,'event_type' TEXT, 'char_name' TEXT, 'ic_name' TEXT, 'message' TEXT NOT NULL);");
    create_chat_events_table.exec();

    QSqlQuery create_connection_events_table("CREATE TABLE IF NOT EXISTS connection_events ('event time' DATETIME DEFAULT CURRENT_TIMESTAMP, 'ipid' TEXT, 'ip_address' TEXT, 'hdid' TEXT);");
    create_connection_events_table.exec();
}

WriterSQL::~WriterSQL()
{
    log_db.close();
}

void WriterSQL::execLogScript(QSqlQuery query)
{
    query.exec();
    QSqlError error = query.lastError();
    if (error.isValid()) {
        qDebug() << "Database Error:" + error.text();
    }
}
