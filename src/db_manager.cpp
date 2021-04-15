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
    QSqlQuery create_ban_table("CREATE TABLE IF NOT EXISTS bans ('ID' INTEGER, 'IPID' TEXT, 'HDID' TEXT, 'IP' TEXT, 'TIME' INTEGER, 'REASON' TEXT, 'DURATION' INTEGER, PRIMARY KEY('ID' AUTOINCREMENT))");
    QSqlQuery create_user_table("CREATE TABLE IF NOT EXISTS users ('ID' INTEGER, 'USERNAME' TEXT, 'SALT' TEXT, 'PASSWORD' TEXT, 'ACL' TEXT, PRIMARY KEY('ID' AUTOINCREMENT))");
}

bool DBManager::isIPBanned(QHostAddress ip)
{
    QSqlQuery query;
    query.prepare("SELECT TIME FROM BANS WHERE IP = ? ORDER BY TIME DESC");
    query.addBindValue(ip.toString());
    query.exec();
    if (query.first()) {
        long long duration = getBanDuration(ip);
        long long ban_time = query.value(0).toLongLong();
        if (duration == -2)
            return true;
        long long current_time = QDateTime::currentDateTime().toSecsSinceEpoch();
        if (ban_time + duration > current_time)
            return true;
        else return false;
    }
    else return false;
}

bool DBManager::isHDIDBanned(QString hdid)
{
    QSqlQuery query;
    query.prepare("SELECT TIME FROM BANS WHERE HDID = ? ORDER BY TIME DESC");
    query.addBindValue(hdid);
    query.exec();
    if (query.first()) {
        long long duration = getBanDuration(hdid);
        long long ban_time = query.value(0).toLongLong();
        if (duration == -2)
            return true;
        long long current_time = QDateTime::currentDateTime().toSecsSinceEpoch();
        if (ban_time + duration > current_time)
            return true;
        else return false;
    }
    else return false;
}

QString DBManager::getBanReason(QHostAddress ip)
{
    QSqlQuery query;
    query.prepare("SELECT REASON FROM BANS WHERE IP = ? ORDER BY TIME DESC");
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
    query.prepare("SELECT REASON FROM BANS WHERE HDID = ? ORDER BY TIME DESC");
    query.addBindValue(hdid);
    query.exec();
    if (query.first()) {
        return query.value(0).toString();
    }
    else {
        return "Ban reason not found.";
    }
}

long long DBManager::getBanDuration(QString hdid)
{
    QSqlQuery query;
    query.prepare("SELECT DURATION FROM BANS WHERE HDID = ? ORDER BY TIME DESC");
    query.addBindValue(hdid);
    query.exec();
    if (query.first()) {
        return query.value(0).toLongLong();
    }
    else {
        return -1;
    }
}

long long DBManager::getBanDuration(QHostAddress ip)
{
    QSqlQuery query;
    query.prepare("SELECT DURATION FROM BANS WHERE IP = ? ORDER BY TIME DESC");
    query.addBindValue(ip.toString());
    query.exec();
    if (query.first()) {
        return query.value(0).toLongLong();
    }
    else {
        return -1;
    }
}


int DBManager::getBanID(QString hdid)
{
    QSqlQuery query;
    query.prepare("SELECT ID FROM BANS WHERE HDID = ?");
    query.addBindValue(hdid);
    query.exec();
    if (query.first()) {
        return query.value(0).toInt();
    }
    else {
        return -1;
    }
}


int DBManager::getBanID(QHostAddress ip)
{
    QSqlQuery query;
    query.prepare("SELECT ID FROM BANS WHERE IP = ?");
    query.addBindValue(ip.toString());
    query.exec();
    if (query.first()) {
        return query.value(0).toInt();
    }
    else {
        return -1;
    }
}

QList<DBManager::BanInfo> DBManager::getRecentBans()
{
    QList<BanInfo> return_list;
    QSqlQuery query;
    query.prepare("SELECT TOP(5) * FROM BANS ORDER BY TIME DESC");
    query.setForwardOnly(true);
    query.exec();
    while (query.next()) {
        BanInfo ban;
        ban.ipid = query.value(0).toString();
        ban.hdid = query.value(1).toString();
        ban.ip = QHostAddress(query.value(2).toString());
        ban.time = static_cast<unsigned long>(query.value(3).toULongLong());
        ban.reason = query.value(4).toString();
        ban.duration = query.value(5).toLongLong();
        return_list.append(ban);
    }
    std::reverse(return_list.begin(), return_list.end());
    return return_list;
}

void DBManager::addBan(BanInfo ban)
{
    QSqlQuery query;
    query.prepare("INSERT INTO BANS(IPID, HDID, IP, TIME, REASON, DURATION) VALUES(?, ?, ?, ?, ?, ?)");
    query.addBindValue(ban.ipid);
    query.addBindValue(ban.hdid);
    query.addBindValue(ban.ip.toString());
    query.addBindValue(QString::number(ban.time));
    query.addBindValue(ban.reason);
    query.addBindValue(ban.duration);
    if (!query.exec())
        qDebug() << "SQL Error:" << query.lastError().text();
}

bool DBManager::invalidateBan(int id)
{
    QSqlQuery ban_exists;
    ban_exists.prepare("SELECT DURATION FROM bans WHERE ID = ?");
    ban_exists.addBindValue(id);
    ban_exists.exec();

    if (!ban_exists.first())
        return false;

    QSqlQuery query;
    query.prepare("UPDATE bans SET DURATION = 0 WHERE ID = ?");
    query.addBindValue(id);
    query.exec();
    return true;
}

bool DBManager::createUser(QString username, QString salt, QString password, unsigned long long acl)
{
    QSqlQuery username_exists;
    username_exists.prepare("SELECT ACL FROM users WHERE USERNAME = ?");
    username_exists.addBindValue(username);
    username_exists.exec();

    if (username_exists.first())
        return false;

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

    return true;
}

bool DBManager::deleteUser(QString username)
{
    QSqlQuery username_exists;
    username_exists.prepare("SELECT ACL FROM users WHERE USERNAME = ?");
    username_exists.addBindValue(username);
    username_exists.exec();

    if (username_exists.first())
        return false;

    QSqlQuery query;
    query.prepare("DELETE FROM users WHERE USERNAME = ?");
    username_exists.addBindValue(username);
    username_exists.exec();
    return true;
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

    return salted_password == stored_pass;
}

bool DBManager::updateACL(QString username, unsigned long long acl, bool mode)
{
    QSqlQuery username_exists;
    username_exists.prepare("SELECT ACL FROM users WHERE USERNAME = ?");
    username_exists.addBindValue(username);
    username_exists.exec();

    if (!username_exists.first())
        return false;

    unsigned long long old_acl = username_exists.value(0).toULongLong();
    unsigned long long new_acl;
    if (mode) // adding perm
        new_acl = old_acl | acl;
    else // removing perm
        new_acl = old_acl & ~acl;
    if (acl == 0) // Allow clearing all perms via adding perm "NONE"
        new_acl = 0;

    QSqlQuery update_acl;
    update_acl.prepare("UPDATE users SET ACL = ? WHERE USERNAME = ?");
    update_acl.addBindValue(new_acl);
    update_acl.addBindValue(username);
    update_acl.exec();
    return true;
}

QStringList DBManager::getUsers()
{
    QStringList users;

    QSqlQuery query("SELECT USERNAME FROM users ORDER BY ID");
    while (query.next()) {
        users.append(query.value(0).toString());
    }

    return users;
}

QList<DBManager::BanInfo> DBManager::getBanInfo(QString lookup_type, QString id)
{
    QList<BanInfo> return_list;
    QSqlQuery query;
    QList<BanInfo> invalid;
    if (lookup_type == "banid") {
        query.prepare("SELECT * FROM BANS WHERE ID = ?");
    }
    else if (lookup_type == "hdid") {
        query.prepare("SELECT * FROM BANS WHERE HDID = ?");
    }
    else if (lookup_type == "ipid") {
        query.prepare("SELECT * FROM BANS WHERE IPID = ?");
    }
    else {
        qCritical("Invalid ban lookup type!");
        return invalid;
    }
    query.addBindValue(id);
    query.setForwardOnly(true);
    query.exec();

    while (query.next()) {
        BanInfo ban;
        ban.ipid = query.value(0).toString();
        ban.hdid = query.value(1).toString();
        ban.ip = QHostAddress(query.value(2).toString());
        ban.time = static_cast<unsigned long>(query.value(3).toULongLong());
        ban.reason = query.value(4).toString();
        ban.duration = query.value(5).toLongLong();
        return_list.append(ban);
    }
    std::reverse(return_list.begin(), return_list.end());
    return return_list;
}

DBManager::~DBManager()
{
    db.close();
}
