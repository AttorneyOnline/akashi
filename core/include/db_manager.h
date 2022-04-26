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

#define DB_VERSION 1

#include <QDateTime>
#include <QFileInfo>
#include <QHostAddress>
#include <QMessageAuthenticationCode>
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>

#include "include/acl_roles_handler.h"

/**
 * @brief A class used to handle database interaction.
 *
 * @details This class offers a layer above the QSqlDatabase class to manage semi-critical data.
 * Contrast with Logger, which dumbs its data into simple `.log` files.
 *
 * The DBManager handles user data, keeping track of only 'special' persons who are handled
 * differently than the average user.
 * This comes in two forms, when the user's client is banned, and when the user is a moderator.
 */
class DBManager : public QObject
{
    Q_OBJECT

  public:
    /**
     * @brief Constructor for the DBManager class.
     *
     * @details Creates a database file at `config/akashi.db`, and creates two tables in it:
     * one for banned clients, and one for authorised users / moderators.
     */
    DBManager();

    /**
     * @brief Destructor for the DBManager class. Closes the underlying database.
     */
    ~DBManager();

    /**
     * @brief Checks if there is a record in the Bans table with the given IPID.
     *
     * @param ipid The IPID to check if it is banned.
     *
     * @return A pair of values:
     * * First, a `bool` that is true if the query could return at least one such record.
     * * Then, a `QString` that is the reason for the ban.
     */
    QPair<bool, QString> isIPBanned(QString ipid);

    /**
     * @brief Checks if there is a record in the Bans table with the given hardware ID.
     *
     * @param hdid The hardware ID to check if it is banned.
     *
     * @return A pair of values:
     * * First, a `bool` that is true if the query could return at least one such record.
     * * Then, a `QString` that is the reason for the ban.
     */
    QPair<bool, QString> isHDIDBanned(QString hdid);

    /**
     * @brief Gets the ID number of a given ban.
     *
     * @param ip The IP address whose associated ban's ID we need.
     *
     * @return The ID of the ban if the IP address is actually banned,
     * or `-1` if the IP address is not actually banned.
     */
    int getBanID(QHostAddress ip);

    /**
     * @overload
     */
    int getBanID(QString hdid);

    /**
     * @brief Details about a ban.
     */
    struct BanInfo
    {
        QString ipid;       //!< The banned user's IPID.
        QHostAddress ip;    //!< The banned user's IP.
        QString hdid;       //!< The banned user's hardware ID.
        unsigned long time; //!< The time the ban was registered.
        QString reason;     //!< The reason given for the ban by the moderator who registered it.
        long long duration; //!< The duration of the ban, in seconds.
        int id;             //!< The unique ID of the ban.
        QString moderator;  //!< The moderator who issued the ban.
    };

    /**
     * @brief Gets the last five bans made on the server.
     *
     * @return See brief description.
     */
    QList<BanInfo> getRecentBans();

    /**
     * @brief Registers a ban into the database.
     *
     * @param ban The details of the ban.
     */
    void addBan(BanInfo ban);

    /**
     * @brief Sets the duration of a given ban to 0, effectively removing the ban the associated user.
     *
     * @param id The ID of the ban to invalidate.
     *
     * @return False if no such ban exists, true if the invalidation was successful.
     */
    bool invalidateBan(int id);

    /**
     * @brief Creates an authorised user.
     *
     * @param username The username clients can use to log in with.
     * @param salt The salt to obfuscate the password with.
     * @param password The user's password.
     * @param acl The ACL role identifier.
     *
     * @return False if the user already exists, true if the user was successfully created.
     *
     * @see AOClient#cmdLogin and AOClient#cmdLogout for the username and password's contexts.
     * @see ACLRolesHandler for details regarding ACL roles and ACL role identifiers.
     */
    bool createUser(QString username, QString salt, QString password, QString acl);

    /**
     * @brief Deletes an authorised user from the database.
     *
     * @param username The username whose associated user to delete.
     *
     * @return False if the user didn't even exist, true if the user was successfully deleted.
     */
    bool deleteUser(QString username);

    /**
     * @brief Gets the ACL role of a given user.
     *
     * @param username The authorised user's name.
     *
     * @return The name identifier of a ACL role.
     *
     * @see ACLRolesHandler for details about ACL roles.
     */
    QString getACL(QString f_username);

    /**
     * @brief Authenticates a given user.
     *
     * @param username The username of the user trying to log in.
     * @param password The password of the user.
     *
     * @return True if the salted version of the inputted password matches the one stored in the user's record,
     * false if the user does not exist in the records, of if the passwords don't match.
     */
    bool authenticate(QString username, QString password);

    /**
     * @brief Updates the ACL role identifier of a given user.
     *
     * @details This function **DOES NOT** modify the ACL role itself. It is simply an identifier that determines which ACL role the user is linked to.
     *
     * @param username The username of the user to be updated.
     *
     * @param acl The ACL role identifier.
     *
     * @return True if the modification was successful, false if the user does not exist in the records.
     */
    bool updateACL(QString username, QString acl);

    /**
     * @brief Returns a list of the recorded users' usernames, ordered by ID.
     *
     * @return See brief description.
     */
    QStringList getUsers();

    /**
     * @brief Gets information on a ban.
     *
     * @param lookup_type The type of ID to search
     *
     * @param id A Ban ID, IPID, or HDID to search for
     */
    QList<BanInfo> getBanInfo(QString lookup_type, QString id);

    /**
     * @brief Updates a ban.
     *
     * @param ban_id The ID of the ban to update.
     *
     * @param field The field to update, either "reason" or "duration".
     *
     * @param updated_info The info to update the field to.
     *
     * @return True if the modification was successful.
     */
    bool updateBan(int ban_id, QString field, QVariant updated_info);

    /**
     * @brief Updates the password of the given user.
     *
     * @param username The username to change.
     *
     * @param password The new password to change to.
     *
     * @return True if the password change was successful.
     */
    bool updatePassword(QString username, QString password);

  private:
    /**
     * @brief The name of the database connection driver.
     */
    const QString DRIVER;

    /**
     * @note Unused.
     */
    const QString CONN_NAME;

    /**
     * @note Unused.
     */
    void openDB();

    /**
     * @brief The backing database that stores user details.
     */
    QSqlDatabase db;

    /**
     * @brief The current server DB version.
     */
    int db_version;

    /**
     * @brief checkVersion Checks the current server DB version.
     *
     * @return Returns the server DB version.
     */
    int checkVersion();

    /**
     * @brief updateDB Updates the server DB to the latest version.
     *
     * @param current_version The current DB version.
     */
    void updateDB(int current_version);
};

#endif // BAN_MANAGER_H
