#ifndef ACL_ROLES_HANDLER_H
#define ACL_ROLES_HANDLER_H

#include <QFlags>
#include <QHash>
#include <QObject>

class ACLRole
{
  public:
    /**
     * @brief This enum is used to specify permissions of a role.
     */
    enum Permission : unsigned int
    {
        NONE = 0,
        KICK = 1 << 0,
        BAN = 1 << 1,
        BGLOCK = 1 << 2,
        MODIFY_USERS = 1 << 3,
        CM = 1 << 4,
        GLOBAL_TIMER = 1 << 5,
        EVI_MOD = 1 << 6,
        MOTD = 1 << 7,
        ANNOUNCE = 1 << 8,
        MODCHAT = 1 << 9,
        MUTE = 1 << 10,
        UNCM = 1 << 11,
        SAVETEST = 1 << 12,
        FORCE_CHARSELECT = 1 << 13,
        BYPASS_LOCKS = 1 << 14,
        IGNORE_BGLIST = 1 << 15,
        SEND_NOTICE = 1 << 16,
        JUKEBOX = 1 << 17,
        SUPER = 0xffffffff,
    };
    Q_DECLARE_FLAGS(Permissions, Permission);

    /**
     * @brief Shared read-only captions for each permissions.
     *
     * @see ACLRoleHandler#loadFile and ACLRoleHandler#saveFile
     */
    static const QHash<ACLRole::Permission, QString> PERMISSION_CAPTIONS;

    /**
     * @brief Constructs a role without any permissions.
     */
    ACLRole();

    /**
     * @brief Constructs a role of the given permissions.
     *
     * @param f_permissions The permissions to which
     */
    ACLRole(ACLRole::Permissions f_permissions);

    /**
     * @brief Destroys the role.
     */
    ~ACLRole();

    /**
     * @brief Returns the permission flags for this role.
     *
     * @return Permission flags.
     */
    ACLRole::Permissions getPermissions() const;

    /**
     * @brief Checks if a given permission is set.
     *
     * @param f_permission The permission flag to check.
     *
     * @return True if the permission is set, false otherwise.
     */
    bool checkPermission(ACLRole::Permission f_permission) const;

    /**
     * @brief Sets the permission if f_mode is true or unsets if f_mode is false.
     *
     * @param f_permission The permission flag to set.
     *
     * @param f_mode If true, will set the flag, unsets otherwise.
     */
    void setPermission(ACLRole::Permission f_permission, bool f_mode);

    /**
     * @brief Sets the permission flags to the given permission flags.
     *
     * @param f_permissions The permission flags to set to.
     */
    void setPermissions(ACLRole::Permissions f_permissions);

  private:
    /**
     * @brief The permission flags of the role.
     */
    ACLRole::Permissions m_permissions;
};
Q_DECLARE_METATYPE(ACLRole::Permission)

class ACLRolesHandler : public QObject
{
    Q_OBJECT

  public:
    /**
     * @brief The identifier of the NONE role.
     */
    static const QString NONE_ID;

    /**
     * @brief The identifier of the SUPER role.
     */
    static const QString SUPER_ID;

    /**
     * @brief Constructs a role handler with parent object.
     *
     * @details The role handler starts off empty without any roles known with the exception of the shared read-only roles.
     *
     * The role handler do not load or save roles automatically.
     *
     * Multiple role handlers can exist simultaneously with different roles and these same roles may have entirely different permissions.
     *
     * @param parent Qt-based parent.
     */
    ACLRolesHandler(QObject *parent = nullptr);

    /**
     * Destroys the role handler.
     */
    ~ACLRolesHandler();

    /**
     * @brief Checks if a role with the given identifier exists.
     *
     * @param f_id The identifier of the role. The identifier is not case-sensitive.
     *
     * @return True if the role exists, false otherwise.
     */
    bool roleExists(QString f_id);

    /**
     * @brief Returns a role with the given identifier. If the role does not exist, a default constructed role will be returned.
     *
     * @param f_id The identifier of the role. The identifier is not case-sensitive.
     *
     * @return A role.
     */
    ACLRole getRoleById(QString f_id);

    /**
     * @brief Inserts a role with the given identifier. If a role already exists, it will be overwritten. Read-only roles cannot be replaced and will return false.
     *
     * @param f_id The identifier of the role. The identifier is not case-sensitive.
     *
     * @param f_role The role to insert.
     *
     * @return True if the role was inserted, false otherwise.
     */
    bool insertRole(QString f_id, ACLRole f_role);

    /**
     * @brief Removes a role of the given identifier. Read-only roles cannot be removed and will return false.
     *
     * @param f_id The identifier of the role. The identifier is not case-sensitive.
     *
     * @return True if the role was removed, false otherwise.
     */
    bool removeRole(QString f_id);

    /**
     * @brief Removes all roles.
     */
    void clearRoles();

    /**
     * @brief Clear the current roles and load the roles from the given file. The file must be INI format compatible.
     *
     * @param f_filename The filename may have no path, a relative path or an absolute path.
     *
     * @return True if successfull, false otherwise.
     */
    bool loadFile(QString f_filename);

    /**
     * @brief Save the current roles to the given file. The file is saved to the INI format.
     *
     * @warning This will completely overwrite the given file.
     *
     * @param f_filename The filename may have no path, a relative path or an absolute path.
     *
     * @return True if successfull, false otherwise.
     */
    bool saveFile(QString f_filename);

  private:
    /**
     * @brief Shared read-only standard roles with the appropriate permissions.
     */
    static const QHash<QString, ACLRole> readonly_roles;

    /**
     * @brief The roles of the handler.
     */
    QHash<QString, ACLRole> m_roles;
};

#endif // ACL_ROLES_HANDLER_H
