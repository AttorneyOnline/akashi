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
     * @brief Overloaded function for getBanReason(QHostAddress). Returns based on HDID instead of IP.
     *
     * @param hdid The hardware ID whose ban reason needs to be returned.
     *
     * @return The ban reason if the hardware ID is actually banned,
     * or `"Ban reason not found."` if the hardware ID is not actually banned.
     */
    QString getBanReason(QString hdid);

    /**
     * @brief Records a ban for the give IPID-IP address-hardware ID combination.
     *
     * @param ipid The IPID to ban.
     * @param ip The IP address to ban. The source should be the same as with the IPID
     * (i.e., they should point to the same user).
     * @param hdid The hardware ID to ban. Once again, should point to the same user as the IPID and the IP address.
     * @param time The number of seconds to ban the target for.
     * @param reason The reason the target was banned.
     */
    void addBan(QString ipid, QHostAddress ip, QString hdid, unsigned long time, QString reason);

    /**
     * @brief Creates an authorised user.
     *
     * @param username The username clients can use to log in with.
     * @param salt The salt to obfuscate the password with.
     * @param password The user's password.
     * @param acl The user's authority bitflag -- what special privileges does the user have.
     *
     * @return False if the user already exists, true if the user was successfully created.
     *
     * @see AOClient::cmdLogin() and AOClient::cmdLogout() for the username and password's contexts.
     * @see AOClient::ACLFlags for the potential special privileges a user may have.
     */
    bool createUser(QString username, QString salt, QString password, unsigned long long acl);

    /**
     * @brief Gets the privileges of a given user.
     *
     * @param moderator_name The authorised user's name.
     *
     * @return `0` if `moderator_name` is empty, `0` if such user does not exist in the Users table,
     * or the primitive representation of an AOClient::ACLFlags privilege matrix if neither of the above are true.
     *
     * @see AOClient::ACLFlags for the potential privileges a user may have.
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
     * @brief Updates the privileges of a given user.
     *
     * @details This function can add or remove privileges as needed.
     * `acl` determines what privileges are modified, while `mode` determines whether said privileges are
     * added or removed.
     *
     * `acl` **is not** the user's current privileges *or* the sum privileges you want for the user at the end
     * -- it is the 'difference' between the user's current and desired privileges.
     *
     * If `acl` is `"NONE"`, then no matter the mode, the user's privileges are cleared.
     *
     * For some practical examples, consult this example table:
     *
     * | Starting privileges |    `acl`    | `mode`  | Resulting privileges |
     * | ------------------: | :---------: | :-----: | :------------------- |
     * | KICK                | BAN         | `TRUE`  | KICK, BAN            |
     * | BAN, KICK           | BAN         | `TRUE`  | KICK, BAN            |
     * | KICK                | BAN, BGLOCK | `TRUE`  | KICK, BAN, BGLOCK    |
     * | BGLOCK, BAN, KICK   | NONE        | `TRUE`  | NONE                 |
     * | KICK                | BAN         | `FALSE` | KICK                 |
     * | BAN, KICK           | BAN         | `FALSE` | KICK                 |
     * | BGLOCK, BAN, KICK   | BAN, BGLOCK | `FALSE` | KICK                 |
     * | BGLOCK, BAN, KICK   | NONE        | `FALSE` | NONE                 |
     *
     * @param username The username of the user whose privileges should be updated.
     * @param acl The primitive representation of the privilege matrix being modified.
     * @param mode If true, the privileges described in `acl` are *added* to the user;
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
