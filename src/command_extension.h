#ifndef COMMAND_EXTENSION_H
#define COMMAND_EXTENSION_H

#include <QMap>
#include <QObject>
#include <QString>
#include <QVector>

#include "acl_roles_handler.h"

class CommandExtension
{
  public:
    /**
     * @brief Constructs a null command extension.
     */
    CommandExtension();

    /**
     * @brief Constructs a command extension with the given command name.
     *
     * @param f_command_name The command's name.
     */
    CommandExtension(QString f_command_name);

    /**
     * @brief Destroys the command extension.
     */
    ~CommandExtension();

    /**
     * @brief Returns the command's name.
     *
     * @details The command's name act as a possible identifier to determine whatever the command extension matches a command or not.
     */
    QString getCommandName() const;

    /**
     * @brief Sets the command name.
     *
     * @param f_command_name The command's name.
     */
    void setCommandName(QString f_command_name);

    /**
     * @brief Checks if the given alias matches any of the possible alias of the command extension, including the command's name itself.
     *
     * @param f_alias The alias to check.
     *
     * @return True if the alias matches, false otherwise.
     */
    bool checkCommandNameAndAlias(QString f_alias) const;

    /**
     * @brief Returns the aliases of the command.
     */
    QStringList getAliases() const;

    /**
     * @brief Sets the aliases of the command to the given aliases.
     *
     * @param f_aliases The command aliases.
     */
    void setAliases(QStringList f_aliases);

    /**
     * @brief Returns the list of permissions. If the permissions are not set or empty, returns f_defaultPermissions.
     *
     * @param f_defaultPermissions A list of permissions to return if the extensions's permissions are not set or empty.
     */
    QVector<ACLRole::Permission> getPermissions(QVector<ACLRole::Permission> f_defaultPermissions) const;

    /**
     * @brief Returns the list of permissions.
     */
    QVector<ACLRole::Permission> getPermissions() const;

    /**
     * @brief Sets the list of permissions to the given list of permissions.
     *
     * @param f_permissions A list of permissions.
     */
    void setPermissions(QVector<ACLRole::Permission> f_permissions);

    /**
     * @brief Sets the list of permissions based on their captions.
     *
     * @param f_captions
     *
     * @see ACLRole#PERMISSION_CAPTIONS
     */
    void setPermissionsByCaption(QStringList f_captions);

  private:
    /**
     * @brief The command name to which the extension is loosely associated to.
     */
    QString m_command_name;

    /**
     * @brief A list of aliases for the command.
     */
    QStringList m_aliases;

    /**
     * @brief A list containing both the command's name and the list of aliases.
     */
    QStringList m_merged_aliases;

    /**
     * @brief A list of permissions.
     */
    QVector<ACLRole::Permission> m_permissions;

    /**
     * @brief Updates #m_merged_aliases.
     */
    void updateMergedAliases();
};

class CommandExtensionCollection : public QObject
{
    Q_OBJECT

  public:
    /**
     * @brief Constructs a null command extension collection.
     *
     * @details The collection does load extensions automatically.
     *
     * @param parent Qt-based parent
     */
    CommandExtensionCollection(QObject *parent = nullptr);

    /**
     * @brief Destroys the collection.
     */
    ~CommandExtensionCollection();

    /**
     * @brief Sets the command name whitelist to the given list.
     *
     * @param f_command_names A list of command name.
     *
     * @see #m_command_name_whitelist
     */
    void setCommandNameWhitelist(QStringList f_command_names);

    /**
     * @brief Returns the list of extensions.
     *
     * @see CommandExtension
     */
    QList<CommandExtension> getExtensions() const;

    /**
     * @brief Checks if a command extension associated to the given command name exists.
     *
     * @param f_command_name The target command name.
     *
     * @return True if the command extension exists, false otherwise.
     */
    bool containsExtension(QString f_command_name) const;

    /**
     * @brief Returns a command extension associated to the given command name. If no command extension is associated to the command name, returns a null command extension.
     *
     * @param f_command_name The target command name.
     *
     * @return Returns a command extension.
     */
    CommandExtension getExtension(QString f_command_name) const;

    /**
     * @brief Clear the current command extensions and load command extensions from the given file. The file must be of the INI format.
     *
     * @details If the command name whitelist is not empty, only command extensions pertaining may be registered.
     *
     * @param f_filename The path to the file.
     */
    bool loadFile(QString f_filename);

  private:
    /**
     * @brief A list of command names to allow.
     *
     * @see #loadFile
     */
    QStringList m_command_name_whitelist;

    /**
     * @brief A map of extensions associated to a command name.
     */
    QMap<QString, CommandExtension> m_extensions;
};

#endif // COMMAND_EXTENSION_H
