#pragma once

#define DB_VERSION 5

#include "akashi_core_export.h"
#include "core/crypto_helper.h"

#include <QDateTime>
#include <QFileInfo>
#include <QHostAddress>
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>

#include <optional>

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
class AKASHI_CORE_EXPORT DBManager : public QObject
{
    Q_OBJECT

  public:
    /**
     * @brief Constructor for the DBManager class.
     *
     * @details Creates a database file at `config/akashi.db`, and creates two tables in it:
     * one for banned clients, and one for authorised users / moderators.
     */
    explicit DBManager(QSqlDatabase f_database);

    /**
     * @brief Destructor for the DBManager class. Closes the underlying database.
     */
    ~DBManager();

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
     * @brief A stored sanction: applied on connect while active. A timed
     * one is lifted by the scheduler; an untimed one holds until a
     * moderator lifts it by hand.
     */
    struct SanctionInfo
    {
        QString ipid;      //!< The sanctioned user's IPID.
        QString sanction;  //!< The sanction id, for example "muted".
        QString moderator; //!< The moderator who issued the sanction.
        qint64 issued;     //!< When the sanction was issued, in epoch seconds.
        qint64 expires;    //!< When the sanction lifts, in epoch seconds; -1 holds until lifted by hand.
        QString hwid;      //!< The sanctioned user's HWID, so a new IP does not shake the sanction off. May be empty.
        QString data;      //!< Sanction-specific payload, for example the charcurse character list.
    };

    /**
     * @brief Stores a sanction, replacing an earlier one for the same
     * IPID and sanction id.
     */
    void upsertSanction(const SanctionInfo &f_sanction);

    /**
     * @brief Removes a stored sanction.
     */
    void removeSanction(const QString &f_ipid, const QString &f_sanction);

    /**
     * @brief The sanctions still in force for an IPID at the given moment.
     */
    QList<SanctionInfo> sanctionsFor(const QString &f_ipid, qint64 f_now);

    /**
     * @brief The sanctions still in force for a person known by IPID or
     * HWID - the join-time check, so neither identifier shakes one off.
     */
    QList<SanctionInfo> sanctionsForIdentity(const QString &f_ipid, const QString &f_hwid, qint64 f_now);

    /**
     * @brief The stored row for one sanction, if any.
     */
    std::optional<SanctionInfo> sanctionRow(const QString &f_ipid, const QString &f_sanction);

    /**
     * @brief Every stored sanction, expired ones included - the boot pass
     * arms a lift for each and overdue lifts clean themselves up.
     */
    QList<SanctionInfo> allSanctions();

    /**
     * @brief Checks if there is a record in the Bans table with the given IPID.
     *
     * @param ipid The IPID to check if it is banned.
     *
     * @return A pair of values:
     * * First, a `bool` that is true if the query could return at least one such record.
     * * Then, a `QString` that is the reason for the ban.
     */
    std::pair<bool, BanInfo> isIPBanned(QString ipid);

    /**
     * @brief Checks if there is a record in the Bans table with the given hardware ID.
     *
     * @param hdid The hardware ID to check if it is banned.
     *
     * @return A pair of values:
     * * First, a `bool` that is true if the query could return at least one such record.
     * * Then, a `QString` that is the reason for the ban.
     */
    std::pair<bool, BanInfo> isHDIDBanned(QString hdid);

    /**
     * @brief Gets the ID number of a given ban.
     *
     * @param ip The IP address whose associated ban's ID we need.
     *
     * @return The ID of the ban if the IP address is actually banned,
     * or `-1` if the IP address is not actually banned.
     */
    int banId(QHostAddress ip);

    /**
     * @overload
     */
    int banId(QString hdid);

    /**
     * @brief Gets the last five bans made on the server.
     *
     * @return See brief description.
     */
    QList<BanInfo> recentBans();

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
     * @see akashi::ClientSession#cmdLogin and akashi::ClientSession#cmdLogout for the username and password's contexts.
     * @see ACLRolesHandler for details regarding ACL roles and ACL role identifiers.
     */
    bool createUser(QString username, QByteArray salt, QString password, QString acl);

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
    QString acl(QString f_username);

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

    struct Credentials
    {
        QString salt;
        QString stored_hash;
        QString acl_role;
    };

    std::optional<Credentials> fetchCredentials(const QString &f_username);

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
    QStringList users();

    /**
     * @brief Gets information on a ban.
     *
     * @param lookup_type The type of ID to search
     *
     * @param id A Ban ID, IPID, or HDID to search for
     */
    QList<BanInfo> banInfo(QString lookup_type, QString id);

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
     * @brief Shared body of isIPBanned/isHDIDBanned: looks up active bans by a
     * single indexed column, applying the same fail-closed and expiry rules.
     * @param f_column The ban column to match on (a fixed literal, never input).
     * @param f_value The value to match against that column.
     * @param f_what The lowercase label for the fail-closed warning (ipid/hdid).
     */
    std::pair<bool, BanInfo> queryBan(const QString &f_column, const QString &f_value, const char *f_what);

    /**
     * @brief The stand-in ban returned when a ban lookup itself fails, so the
     * connection gate fails closed instead of admitting the client.
     */
    static BanInfo databaseErrorBan();

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
