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

#include <QDebug>
#include <QDateTime>
#include <QHostAddress>
#include <QMessageAuthenticationCode>
#include <QString>
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>

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
class DBManager{
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
     * @brief Checks if there is a record in the Bans table with the given IP address.
     *
     * @param ip The IP address to check if it is banned.
     *
     * @return True if the query could return at least one such record.
     */
    bool isIPBanned(QHostAddress ip);

    /**
     * @brief Checks if there is a record in the Bans table with the given hardware ID.
     *
     * @param hdid The hardware ID to check if it is banned.
     *
     * @return True if the query could return at least one such record.
     */
    bool isHDIDBanned(QString hdid);

    /**
     * @brief Returns the reason the given IP address was banned with.
     *
     * @param ip The IP address whose ban reason needs to be returned.
     *
     * @return The ban reason if the IP address is actually banned,
     * or `"Ban reason not found."` if the IP address is not actually banned.
     */
    QString getBanReason(QHostAddress ip);

    /**
     * @overload
     */
    QString getBanReason(QString hdid);

    /**
     * @brief Returns the reason the given IP address was banned with.
     *
     * @param ip The IP address whose ban duration to get.
     *
     * @return The ban duration if the IP address is actually banned,
     * or `-1` if the IP address is not actually banned.
     */
    long long getBanDuration(QHostAddress ip);

    /**
     * @overload
     */
    long long getBanDuration(QString hdid);

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
    struct BanInfo {
        QString ipid; //!< The banned user's IPID.
        QHostAddress ip; //!< The banned user's IP.
        QString hdid; //!< The banned user's hardware ID.
        unsigned long time; //!< The time the ban was registered.
        QString reason; //!< The reason given for the ban by the moderator who registered it.
        long long duration; //!< The duration of the ban, in seconds.
        int id; //!< The unique ID of the ban.
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
     * @param acl The user's authority bitflag -- what special permissions does the user have.
     *
     * @return False if the user already exists, true if the user was successfully created.
     *
     * @see AOClient::cmdLogin() and AOClient::cmdLogout() for the username and password's contexts.
     * @see AOClient::ACLFlags for the potential special permissions a user may have.
     */
    bool createUser(QString username, QString salt, QString password, unsigned long long acl);

    /**
     * @brief Deletes an authorised user from the database.
     *
     * @param username The username whose associated user to delete.
     *
     * @return False if the user didn't even exist, true if the user was successfully deleted.
     */
    bool deleteUser(QString username);

    /**
     * @brief Gets the permissions of a given user.
     *
     * @param moderator_name The authorised user's name.
     *
     * @return `0` if `moderator_name` is empty, `0` if such user does not exist in the Users table,
     * or the primitive representation of an AOClient::ACLFlags permission matrix if neither of the above are true.
     *
     * @see AOClient::ACLFlags for the potential permissions a user may have.
     */
    unsigned long long getACL(QString moderator_name);

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
     * @brief Updates the permissions of a given user.
     *
     * @details This function can add or remove permissions as needed.
     * `acl` determines what permissions are modified, while `mode` determines whether said permissions are
     * added or removed.
     *
     * `acl` **is not** the user's current permissions *or* the sum permissions you want for the user at the end
     * -- it is the 'difference' between the user's current and desired permissions.
     *
     * If `acl` is `"NONE"`, then no matter the mode, the user's permissions are cleared.
     *
     * For some practical examples, consult this example table:
     *
     * | Starting permissions |    `acl`    | `mode`  | Resulting permissions |
     * | -------------------: | :---------: | :-----: | :-------------------- |
     * | KICK                 | BAN         | `TRUE`  | KICK, BAN             |
     * | BAN, KICK            | BAN         | `TRUE`  | KICK, BAN             |
     * | KICK                 | BAN, BGLOCK | `TRUE`  | KICK, BAN, BGLOCK     |
     * | BGLOCK, BAN, KICK    | NONE        | `TRUE`  | NONE                  |
     * | KICK                 | BAN         | `FALSE` | KICK                  |
     * | BAN, KICK            | BAN         | `FALSE` | KICK                  |
     * | BGLOCK, BAN, KICK    | BAN, BGLOCK | `FALSE` | KICK                  |
     * | BGLOCK, BAN, KICK    | NONE        | `FALSE` | NONE                  |
     *
     * @param username The username of the user whose permissions should be updated.
     * @param acl The primitive representation of the permission matrix being modified.
     * @param mode If true, the permissions described in `acl` are *added* to the user;
     * if false, they are removed instead.
     *
     * @return True if the modification was successful, false if the user does not exist in the records.
     */
    bool updateACL(QString username, unsigned long long acl, bool mode);

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
};

#endif // BAN_MANAGER_H
