#include "core/db_manager.h"

#include "akashi/database_service.h"
#include "akashi/logging_categories.h"

DBManager::DBManager(QSqlDatabase f_database) :
    db(f_database)
{
    db_version = checkVersion();
    // A database without a bans table is brand new; it gets the current
    // schema directly and must never replay the old migrations against it.
    const bool l_fresh = !db.tables().contains(QStringLiteral("bans"));
    QSqlQuery create_ban_table(db);
    create_ban_table.exec("CREATE TABLE IF NOT EXISTS bans ('ID' INTEGER, 'IPID' TEXT, 'HDID' TEXT, 'IP' TEXT, 'TIME' INTEGER, 'REASON' TEXT, 'DURATION' INTEGER, 'MODERATOR' TEXT, PRIMARY KEY('ID' AUTOINCREMENT))");
    QSqlQuery create_user_table(db);
    create_user_table.exec("CREATE TABLE IF NOT EXISTS users ('ID' INTEGER, 'USERNAME' TEXT, 'SALT' TEXT, 'PASSWORD' TEXT, 'ACL' TEXT, PRIMARY KEY('ID' AUTOINCREMENT))");
    QSqlQuery create_sanction_table(db);
    create_sanction_table.exec("CREATE TABLE IF NOT EXISTS sanctions ('ID' INTEGER, 'IPID' TEXT NOT NULL, 'SANCTION' TEXT NOT NULL, 'MODERATOR' TEXT, 'ISSUED' INTEGER, 'EXPIRES' INTEGER NOT NULL, PRIMARY KEY('ID' AUTOINCREMENT), UNIQUE('IPID', 'SANCTION'))");
    // The lookup indexes are part of the current schema; they carry the
    // same names migration 3 uses, so nothing double-creates.
    QSqlQuery(db).exec("CREATE INDEX IF NOT EXISTS bans_ipid_time ON bans(IPID, TIME)");
    QSqlQuery(db).exec("CREATE INDEX IF NOT EXISTS bans_hdid_time ON bans(HDID, TIME)");
    QSqlQuery(db).exec("CREATE INDEX IF NOT EXISTS bans_ip ON bans(IP)");
    QSqlQuery(db).exec("CREATE INDEX IF NOT EXISTS users_username ON users(USERNAME)");
    if (l_fresh) {
        QSqlQuery(db).exec("PRAGMA user_version = " + QString::number(DB_VERSION));
        db_version = DB_VERSION;
    }
    else if (db_version != DB_VERSION) {
        updateDB(db_version);
    }
}

QPair<bool, DBManager::BanInfo> DBManager::isIPBanned(QString ipid)
{
    QSqlQuery query(db);
    query.prepare("SELECT * FROM BANS WHERE IPID = ? ORDER BY TIME DESC");
    query.addBindValue(ipid);
    query.exec();
    // Every ban is checked, an older ban can still be active while newer ones expired.
    BanInfo ban;
    while (query.next()) {
        ban.id = query.value(0).toInt();
        ban.ipid = query.value(1).toString();
        ban.hdid = query.value(2).toString();
        ban.ip = QHostAddress(query.value(3).toString());
        ban.time = static_cast<unsigned long>(query.value(4).toULongLong());
        ban.reason = query.value(5).toString();
        ban.duration = query.value(6).toLongLong();
        ban.moderator = query.value(7).toString();
        if (ban.duration == -2)
            return {true, ban};
        unsigned long current_time = QDateTime::currentDateTime().toSecsSinceEpoch();
        if (ban.time + ban.duration > current_time)
            return {true, ban};
    }
    return {false, ban};
}

QPair<bool, DBManager::BanInfo> DBManager::isHDIDBanned(QString hdid)
{
    QSqlQuery query(db);
    query.prepare("SELECT * FROM BANS WHERE HDID = ? ORDER BY TIME DESC");
    query.addBindValue(hdid);
    query.exec();
    // Every ban is checked, an older ban can still be active while newer ones expired.
    BanInfo ban;
    while (query.next()) {
        ban.id = query.value(0).toInt();
        ban.ipid = query.value(1).toString();
        ban.hdid = query.value(2).toString();
        ban.ip = QHostAddress(query.value(3).toString());
        ban.time = static_cast<unsigned long>(query.value(4).toULongLong());
        ban.reason = query.value(5).toString();
        ban.duration = query.value(6).toLongLong();
        ban.moderator = query.value(7).toString();
        if (ban.duration == -2)
            return {true, ban};
        unsigned long current_time = QDateTime::currentDateTime().toSecsSinceEpoch();
        if (ban.time + ban.duration > current_time)
            return {true, ban};
    }
    return {false, ban};
}

int DBManager::banId(QString hdid)
{
    QSqlQuery query(db);
    query.prepare("SELECT ID FROM BANS WHERE HDID = ? ORDER BY TIME DESC");
    query.addBindValue(hdid);
    query.exec();
    if (query.first()) {
        return query.value(0).toInt();
    }
    else {
        return -1;
    }
}

int DBManager::banId(QHostAddress ip)
{
    QSqlQuery query(db);
    query.prepare("SELECT ID FROM BANS WHERE IP = ? ORDER BY TIME DESC");
    query.addBindValue(ip.toString());
    query.exec();
    if (query.first()) {
        return query.value(0).toInt();
    }
    else {
        return -1;
    }
}

QList<DBManager::BanInfo> DBManager::recentBans()
{
    QList<BanInfo> return_list;
    QSqlQuery query(db);
    query.prepare("SELECT * FROM BANS ORDER BY TIME DESC LIMIT 5");
    query.setForwardOnly(true);
    query.exec();
    while (query.next()) {
        BanInfo ban;
        ban.id = query.value(0).toInt();
        ban.ipid = query.value(1).toString();
        ban.hdid = query.value(2).toString();
        ban.ip = QHostAddress(query.value(3).toString());
        ban.time = static_cast<unsigned long>(query.value(4).toULongLong());
        ban.reason = query.value(5).toString();
        ban.duration = query.value(6).toLongLong();
        ban.moderator = query.value(7).toString();
        return_list.append(ban);
    }
    std::reverse(return_list.begin(), return_list.end());
    return return_list;
}

void DBManager::addBan(BanInfo ban)
{
    QSqlQuery query(db);
    query.prepare("INSERT INTO BANS(IPID, HDID, IP, TIME, REASON, DURATION, MODERATOR) VALUES(?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(ban.ipid);
    query.addBindValue(ban.hdid);
    query.addBindValue(ban.ip.toString());
    query.addBindValue(QVariant::fromValue<qulonglong>(ban.time));
    query.addBindValue(ban.reason);
    query.addBindValue(ban.duration);
    query.addBindValue(ban.moderator);
    if (!query.exec())
        qCDebug(akashiDb) << "SQL Error:" << query.lastError().text();
}

bool DBManager::invalidateBan(int id)
{
    QSqlQuery ban_exists(db);
    ban_exists.prepare("SELECT DURATION FROM bans WHERE ID = ?");
    ban_exists.addBindValue(id);
    ban_exists.exec();

    if (!ban_exists.first())
        return false;

    QSqlQuery query(db);
    query.prepare("UPDATE bans SET DURATION = 0 WHERE ID = ?");
    query.addBindValue(id);
    query.exec();
    return true;
}

bool DBManager::createUser(QString f_username, QByteArray f_salt, QString f_password, QString f_acl)
{
    QSqlQuery username_exists(db);
    username_exists.prepare("SELECT ACL FROM users WHERE USERNAME = ?");
    username_exists.addBindValue(f_username);
    username_exists.exec();

    if (username_exists.first())
        return false;

    QSqlQuery query(db);

    QString salted_password = CryptoHelper::hash_password(f_salt, f_password);

    query.prepare("INSERT INTO users(USERNAME, SALT, PASSWORD, ACL) VALUES(?, ?, ?, ?)");
    query.addBindValue(f_username);
    query.addBindValue(f_salt.toHex());
    query.addBindValue(salted_password);
    query.addBindValue(f_acl);
    query.exec();

    return true;
}

bool DBManager::deleteUser(QString username)
{
    if (username == "root") {
        // To prevent lockout scenarios where an admin may accidentally delete root.
        return false;
    }

    {
        QSqlQuery username_exists(db);
        username_exists.prepare("SELECT EXISTS(SELECT USERNAME FROM users WHERE USERNAME = ?)");
        username_exists.addBindValue(username);
        username_exists.exec();
        username_exists.first();
        // If EXISTS can't find a record, it returns 0.
        if (username_exists.value(0).toInt() == 0)
            // We were unable to locate an entry with this name.
            return false;
    }
    {
        QSqlQuery username_delete(db);
        username_delete.prepare("DELETE FROM users WHERE USERNAME = ?");
        username_delete.addBindValue(username);
        username_delete.exec();
        return true;
    }
}

QString DBManager::acl(QString moderator_name)
{
    if (moderator_name == "")
        return 0;
    QSqlQuery query(db);
    query.prepare("SELECT ACL FROM users WHERE USERNAME = ?");
    query.addBindValue(moderator_name);
    query.exec();
    if (!query.first())
        return 0;
    return query.value(0).toString();
}

bool DBManager::authenticate(QString username, QString password)
{
    QSqlQuery query(db);
    query.prepare("SELECT SALT, PASSWORD FROM users WHERE USERNAME = ?");
    query.addBindValue(username);
    query.exec();
    if (!query.first())
        return false;
    QString salt = query.value(0).toString();
    QString stored_pass = query.value(1).toString();

    QString salted_password = CryptoHelper::hash_password(QByteArray::fromHex(salt.toUtf8()), password);

    const bool l_matches = CryptoHelper::constantTimeEquals(salted_password, stored_pass);

    // Update old-style hashes to new ones on the fly
    if (QByteArray::fromHex(salt.toUtf8()).length() < CryptoHelper::pbkdf2_salt_len && l_matches) {
        updatePassword(username, password);
    }

    return l_matches;
}

std::optional<DBManager::Credentials> DBManager::fetchCredentials(const QString &f_username)
{
    QSqlQuery l_query(db);
    l_query.prepare("SELECT SALT, PASSWORD, ACL FROM users WHERE USERNAME = ?");
    l_query.addBindValue(f_username);
    l_query.exec();
    if (!l_query.first())
        return std::nullopt;
    return Credentials{l_query.value(0).toString(), l_query.value(1).toString(), l_query.value(2).toString()};
}

bool DBManager::updateACL(QString f_username, QString f_acl)
{
    QSqlQuery l_username_exists(db);
    l_username_exists.prepare("SELECT ACL FROM users WHERE USERNAME = ?");
    l_username_exists.addBindValue(f_username);
    l_username_exists.exec();

    if (!l_username_exists.first())
        return false;

    QSqlQuery l_update_acl(db);
    l_update_acl.prepare("UPDATE users SET ACL = ? WHERE USERNAME = ?");
    l_update_acl.addBindValue(f_acl);
    l_update_acl.addBindValue(f_username);
    l_update_acl.exec();
    return true;
}

QStringList DBManager::users()
{
    QStringList users;

    QSqlQuery query(db);
    query.exec("SELECT USERNAME FROM users ORDER BY ID");
    while (query.next()) {
        users.append(query.value(0).toString());
    }

    return users;
}

QList<DBManager::BanInfo> DBManager::banInfo(QString lookup_type, QString id)
{
    QList<BanInfo> return_list;
    QSqlQuery query(db);
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
        ban.id = query.value(0).toInt();
        ban.ipid = query.value(1).toString();
        ban.hdid = query.value(2).toString();
        ban.ip = QHostAddress(query.value(3).toString());
        ban.time = static_cast<unsigned long>(query.value(4).toULongLong());
        ban.reason = query.value(5).toString();
        ban.duration = query.value(6).toLongLong();
        ban.moderator = query.value(7).toString();
        return_list.append(ban);
    }
    std::reverse(return_list.begin(), return_list.end());
    return return_list;
}

bool DBManager::updateBan(int ban_id, QString field, QVariant updated_info)
{
    QSqlQuery query(db);
    if (field == "reason") {
        query.prepare("UPDATE bans SET REASON = ? WHERE ID = ?");
        query.addBindValue(updated_info.toString());
    }
    else if (field == "duration") {
        query.prepare("UPDATE bans SET DURATION = ? WHERE ID = ?");
        query.addBindValue(updated_info.toLongLong());
    }
    query.addBindValue(ban_id);
    if (!query.exec()) {
        qCDebug(akashiDb) << query.lastError();
        return false;
    }
    else {
        return true;
    }
}

bool DBManager::updatePassword(QString username, QString password)
{
    QByteArray salt = CryptoHelper::randbytes(16);
    QString salted_password = CryptoHelper::hash_password(salt, password);

    QSqlQuery query(db);
    query.prepare("UPDATE users SET PASSWORD = ?, SALT = ? WHERE USERNAME = ?");
    query.addBindValue(salted_password);
    query.addBindValue(salt.toHex());
    query.addBindValue(username);
    query.exec();
    return true;
}

int DBManager::checkVersion()
{
    QSqlQuery query(db);
    query.prepare("PRAGMA user_version");
    query.exec();
    if (query.first()) {
        return query.value(0).toInt();
    }
    else {
        return 0;
    }
}

void DBManager::updateDB(int current_version)
{
    switch (current_version) {
    case 0:
        akashi::DatabaseService::applyMigration(db, 1, [](QSqlDatabase &f_db) {
            return QSqlQuery(f_db).exec("ALTER TABLE bans ADD COLUMN MODERATOR TEXT");
        });
        Q_FALLTHROUGH();
    case 1:
        akashi::DatabaseService::applyMigration(db, 2, [](QSqlDatabase &f_db) {
            return QSqlQuery(f_db).exec("UPDATE users SET ACL = 'SUPER' WHERE USERNAME = 'root'");
        });
        Q_FALLTHROUGH();
    case 2:
        // The ban checks run on every connection; without these they scan the whole table.
        akashi::DatabaseService::applyMigration(db, 3, [](QSqlDatabase &f_db) {
            return QSqlQuery(f_db).exec("CREATE INDEX IF NOT EXISTS bans_ipid_time ON bans(IPID, TIME)") &&
                   QSqlQuery(f_db).exec("CREATE INDEX IF NOT EXISTS bans_hdid_time ON bans(HDID, TIME)") &&
                   QSqlQuery(f_db).exec("CREATE INDEX IF NOT EXISTS bans_ip ON bans(IP)") &&
                   QSqlQuery(f_db).exec("CREATE INDEX IF NOT EXISTS users_username ON users(USERNAME)");
        });
        Q_FALLTHROUGH();
    case 3:
        // Timed sanctions outlive sessions: stored here, applied on
        // connect, lifted by the scheduler.
        akashi::DatabaseService::applyMigration(db, 4, [](QSqlDatabase &f_db) {
            return QSqlQuery(f_db).exec("CREATE TABLE IF NOT EXISTS sanctions ('ID' INTEGER, 'IPID' TEXT NOT NULL, 'SANCTION' TEXT NOT NULL, 'MODERATOR' TEXT, 'ISSUED' INTEGER, 'EXPIRES' INTEGER NOT NULL, PRIMARY KEY('ID' AUTOINCREMENT), UNIQUE('IPID', 'SANCTION'))");
        });
        break;
    }
}

void DBManager::upsertSanction(const SanctionInfo &f_sanction)
{
    QSqlQuery query(db);
    query.prepare("INSERT INTO sanctions(IPID, SANCTION, MODERATOR, ISSUED, EXPIRES) VALUES(?, ?, ?, ?, ?) "
                  "ON CONFLICT(IPID, SANCTION) DO UPDATE SET MODERATOR = excluded.MODERATOR, ISSUED = excluded.ISSUED, EXPIRES = excluded.EXPIRES");
    query.addBindValue(f_sanction.ipid);
    query.addBindValue(f_sanction.sanction);
    query.addBindValue(f_sanction.moderator);
    query.addBindValue(f_sanction.issued);
    query.addBindValue(f_sanction.expires);
    if (!query.exec()) {
        qCWarning(akashiDb) << "Could not store sanction:" << query.lastError().text();
    }
}

void DBManager::removeSanction(const QString &f_ipid, const QString &f_sanction)
{
    QSqlQuery query(db);
    query.prepare("DELETE FROM sanctions WHERE IPID = ? AND SANCTION = ?");
    query.addBindValue(f_ipid);
    query.addBindValue(f_sanction);
    query.exec();
}

static QList<DBManager::SanctionInfo> readSanctions(QSqlQuery &query)
{
    QList<DBManager::SanctionInfo> sanctions;
    while (query.next()) {
        DBManager::SanctionInfo sanction;
        sanction.ipid = query.value(0).toString();
        sanction.sanction = query.value(1).toString();
        sanction.moderator = query.value(2).toString();
        sanction.issued = query.value(3).toLongLong();
        sanction.expires = query.value(4).toLongLong();
        sanctions.append(sanction);
    }
    return sanctions;
}

QList<DBManager::SanctionInfo> DBManager::sanctionsFor(const QString &f_ipid, qint64 f_now)
{
    QSqlQuery query(db);
    query.prepare("SELECT IPID, SANCTION, MODERATOR, ISSUED, EXPIRES FROM sanctions WHERE IPID = ? AND EXPIRES > ?");
    query.addBindValue(f_ipid);
    query.addBindValue(f_now);
    query.exec();
    return readSanctions(query);
}

QList<DBManager::SanctionInfo> DBManager::allSanctions()
{
    QSqlQuery query(db);
    query.prepare("SELECT IPID, SANCTION, MODERATOR, ISSUED, EXPIRES FROM sanctions");
    query.exec();
    return readSanctions(query);
}

DBManager::~DBManager()
{
    db.close();
}
