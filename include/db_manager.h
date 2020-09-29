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
#ifndef BAN_MANAGER_H
#define BAN_MANAGER_H

#include "include/aoclient.h"

#include <QDebug>
#include <QHostAddress>
#include <QMessageAuthenticationCode>
#include <QString>
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>

class DBManager{
public:
    DBManager();
    ~DBManager();

    bool isIPBanned(QHostAddress ip);
    bool isHDIDBanned(QString hdid);

    QString getBanReason(QHostAddress ip);
    QString getBanReason(QString hdid);

    void addBan(QString ipid, QHostAddress ip, QString hdid, unsigned long time, QString reason);

    void createUser(QString username, QString salt, QString password, unsigned long long acl);
    unsigned long long getACL(QString moderator_name);
    bool authenticate(QString username, QString password);

private:
    const QString DRIVER;
    const QString CONN_NAME;

    void openDB();

    QSqlDatabase db;
};

#endif // BAN_MANAGER_H
