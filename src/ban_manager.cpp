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
#include "include/ban_manager.h"

BanManager::BanManager() :
    DRIVER("QSQLITE")
{
    db = QSqlDatabase::addDatabase(DRIVER);
    db.setDatabaseName("config/bans.db");
    if (!db.open())
        qCritical() << "Database Error:" << db.lastError();
    QSqlQuery create_table_query("CREATE TABLE IF NOT EXISTS bans ('ID' INTEGER, 'IPID' TEXT, 'HDID' TEXT, 'IP' TEXT, 'TIME' INTEGER, 'REASON' TEXT, PRIMARY KEY('ID' AUTOINCREMENT));");
}

bool BanManager::isIPBanned(QHostAddress ip)
{
    QSqlQuery query;
    query.prepare("SELECT ID FROM BANS WHERE IP = ?");
    query.addBindValue(ip.toString());
    return query.first();
}

bool BanManager::isHDIDBanned(QString hdid)
{
    QSqlQuery query;
    query.prepare("SELECT ID FROM BANS WHERE HDID = ?");
    query.addBindValue(hdid);
    return query.first();
}

QString BanManager::getBanReason(QHostAddress ip)
{
    QSqlQuery query;
    query.prepare("SELECT ID FROM BANS WHERE IP = ?");
    query.addBindValue(ip.toString());
    if (query.first()) {
        return query.value(0).toString();
    }
    else {
        return "Ban reason not found.";
    }
}

QString BanManager::getBanReason(QString hdid)
{
    QSqlQuery query;
    query.prepare("SELECT ID FROM BANS WHERE HDID = ?");
    query.addBindValue(hdid);
    if (query.first()) {
        return query.value(0).toString();
    }
    else {
        return "Ban reason not found.";
    }
}

void BanManager::addBan(QString ipid, QHostAddress ip, QString hdid, unsigned long time, QString reason)
{
    QSqlQuery query;
    query.prepare("INSERT INTO BANS(IPID, HDID, IP, TIME, REASON) VALUES(?, ?, ?, ?, ?)");
    query.addBindValue(ipid);
    query.addBindValue(hdid);
    query.addBindValue(ip.toString());
    query.addBindValue(QString::number(time));
    query.addBindValue(reason);
    query.exec();
}

BanManager::~BanManager()
{
    db.close();
}
