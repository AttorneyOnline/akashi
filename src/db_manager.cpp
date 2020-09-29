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
#include "include/db_manager.h"

DBManager::DBManager() :
    DRIVER("QSQLITE")
{
    db = QSqlDatabase::addDatabase(DRIVER);
    db.setDatabaseName("config/akashi.db");
    if (!db.open())
        qCritical() << "Database Error:" << db.lastError();
    QSqlQuery create_ban_table("CREATE TABLE IF NOT EXISTS bans ('ID' INTEGER, 'IPID' TEXT, 'HDID' TEXT, 'IP' TEXT, 'TIME' INTEGER, 'REASON' TEXT, PRIMARY KEY('ID' AUTOINCREMENT))");
    QSqlQuery create_user_table("CREATE TABLE IF NOT EXISTS users ('ID' INTEGER, 'USERNAME' TEXT, 'SALT' TEXT, 'PASSWORD' TEXT, 'ACL' TEXT, PRIMARY KEY('ID' AUTOINCREMENT))");
}

bool DBManager::isIPBanned(QHostAddress ip)
{
    QSqlQuery query;
    query.prepare("SELECT ID FROM BANS WHERE IP = ?");
    query.addBindValue(ip.toString());
    query.exec();
    return query.first();
}

bool DBManager::isHDIDBanned(QString hdid)
{
    QSqlQuery query;
    query.prepare("SELECT ID FROM BANS WHERE HDID = ?");
    query.addBindValue(hdid);
    query.exec();
    return query.first();
}

QString DBManager::getBanReason(QHostAddress ip)
{
    QSqlQuery query;
    query.prepare("SELECT REASON FROM BANS WHERE IP = ?");
    query.addBindValue(ip.toString());
    query.exec();
    if (query.first()) {
        return query.value(0).toString();
    }
    else {
        return "Ban reason not found.";
    }
}

QString DBManager::getBanReason(QString hdid)
{
    QSqlQuery query;
    query.prepare("SELECT REASON FROM BANS WHERE HDID = ?");
    query.addBindValue(hdid);
    query.exec();
    if (query.first()) {
        return query.value(0).toString();
    }
    else {
        return "Ban reason not found.";
    }
}

void DBManager::addBan(QString ipid, QHostAddress ip, QString hdid, unsigned long time, QString reason)
{
    QSqlQuery query;
    query.prepare("INSERT INTO BANS(IPID, HDID, IP, TIME, REASON) VALUES(?, ?, ?, ?, ?)");
    query.addBindValue(ipid);
    query.addBindValue(hdid);
    query.addBindValue(ip.toString());
    query.addBindValue(QString::number(time));
    query.addBindValue(reason);
    if (!query.exec())
        qDebug() << "SQL Error:" << query.lastError().text();
}

void DBManager::createUser(QString username, QString salt, QString password, unsigned long long acl)
{
    QSqlQuery query;

    QString salted_password;
    QMessageAuthenticationCode hmac(QCryptographicHash::Sha256);
    hmac.setKey(salt.toUtf8());
    hmac.addData(password.toUtf8());
    salted_password = hmac.result().toHex();

    query.prepare("INSERT INTO users(USERNAME, SALT, PASSWORD, ACL) VALUES(?, ?, ?, ?)");
    query.addBindValue(username);
    query.addBindValue(salt);
    query.addBindValue(salted_password);
    query.addBindValue(acl);
    query.exec();

    qDebug() << "Created user" << username << "with password" << password << "and salted with value" << salt << ": stored as" << salted_password;
}

unsigned long long DBManager::getACL(QString moderator_name)
{
    if (moderator_name == "")
        return 0;
    QSqlQuery query("SELECT ACL FROM users WHERE USERNAME = ?");
    query.addBindValue(moderator_name);
    query.exec();
    if (!query.first())
        return 0;
    return query.value(0).toULongLong();
}

bool DBManager::authenticate(QString username, QString password)
{
    QSqlQuery query_salt("SELECT SALT FROM users WHERE USERNAME = ?");
    query_salt.addBindValue(username);
    query_salt.exec();
    if (!query_salt.first())
        return false;
    QString salt = query_salt.value(0).toString();

    QString salted_password;
    QMessageAuthenticationCode hmac(QCryptographicHash::Sha256);
    hmac.setKey(salt.toUtf8());
    hmac.addData(password.toUtf8());
    salted_password = hmac.result().toHex();

    QSqlQuery query_pass("SELECT PASSWORD FROM users WHERE SALT = ?");
    query_pass.addBindValue(salt);
    query_pass.exec();
    if (!query_pass.first())
        return false;
    QString stored_pass = query_pass.value(0).toString();

    qDebug() << "Found DB entry Salt:" << salt << "Stored Password:" << stored_pass << "Calculated Password:" << salted_password;

    return salted_password == stored_pass;
}

bool DBManager::updateACL(QString username, unsigned long long acl)
{
    QSqlQuery username_exists;
    username_exists.prepare("SELECT ACL FROM users WHERE USERNAME = ?");
    username_exists.addBindValue(username);
    username_exists.exec();

    if (!username_exists.first())
        return false;

    unsigned long long old_acl = username_exists.value(0).toULongLong();
    unsigned long long new_acl = acl | old_acl;
    if (acl == 0) // Allow clearing all perms via adding perm "NONE"
        new_acl = 0;

    QSqlQuery update_acl;
    update_acl.prepare("UPDATE users SET ACL = ? WHERE USERNAME = ?");
    update_acl.addBindValue(new_acl);
    update_acl.addBindValue(username);
    update_acl.exec();
    return true;
}

DBManager::~DBManager()
{
    db.close();
}
