//////////////////////////////////////////////////////////////////////////////////////
//    akashi - a server for Attorney Online 2                                       //
//    Copyright (C) 2020  scatterflower                                             //
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
#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#define CONFIG_VERSION 1

#include "data_types.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QUrl>
#include <QMetaEnum>

/**
 * @brief The config file handler class.
 */
class ConfigManager {

  public:
    /**
     * @brief Verifies the server configuration, confirming all required files/directories exist and are valid.
     *
     * @return True if the server configuration was verified, false otherwise.
     */
    static bool verifyServerConfig();

    /**
     * @brief Begins loading server settings from config.ini.
     *
     * @return True if the settings were successfully loaded, false otherwise.
     */
    static bool loadConfigSettings();

    /**
     * @brief Reloads server settings from config.ini that can be reloaded. This is also called by `loadConfigSettings` to finish setting loading.
     *
     * @return True if the server settings were successfully reloaded, false otherwise.
     *
     * @see loadConfigSettings
     */
    static bool reloadConfigSettings();

    /**
     * @brief Returns true if the server should advertise to the master server.
     *
     * @return See short description.
     */
    static bool advertiseServer();

    /**
     * @brief Returns the maximum number of players the server will allow.
     *
     * @return See short description.
     */
    static int maxPlayers();

    /**
     * @brief Returns the IP of the master server to advertise to.
     *
     * @return See short description.
     */
    static QString masterServerIP();

    /**
     * @brief Returns the port of the master server to advertise to.
     *
     * @return See short description.
     */
    static int masterServerPort();

    /**
     * @brief Returns the port to listen for connections on.
     *
     * @return See short description.
     */
    static int serverPort();

    /**
     * @brief Returns the server description.
     *
     * @return See short description.
     */
    static QString serverDescription();

    /**
     * @brief Returns the server name.
     *
     * @return See short description.
     */
    static QString serverName();

    /**
     * @brief Returns the server's Message of the Day.
     *
     * @return See short description.
     */
    static QString motd();

    /**
     * @brief Returns true if the server should accept webAO connections.
     *
     * @return See short description.
     */
    static bool webaoEnabled();

    /**
     * @brief Returns the port to listen for webAO connections on.
     *
     * @return See short description.
     */
    static int webaoPort();

    /**
     * @brief Returns the server's authorization type.
     *
     * @return See short description.
     */
    static DataTypes::AuthType authType();

    /**
     * @brief Returns the server's moderator password.
     *
     * @return See short description.
     */
    static QString modpass();

    /**
     * @brief Returns the server's log buffer length.
     *
     * @return See short description.
     */
    static int logBuffer();

    /**
     * @brief Returns the server's logging type.
     *
     * @return See short description.
     */
    static DataTypes::LogType loggingType();

    /**
     * @brief Returns true if the server should advertise to the master server.
     *
     * @return See short description.
     */
    static int maxStatements();

    /**
     * @brief Returns the maximum number of permitted connections from the same IP.
     *
     * @return See short description.
     */
    static int multiClientLimit();

    /**
     * @brief Returns the maximum number of characters a message can contain.
     *
     * @return See short description.
     */
    static int maxCharacters();

    /**
     * @brief Returns the duration of the message floodguard.
     *
     * @return See short description.
     */
    static int messageFloodguard();

    /**
     * @brief Returns the URL where the server should retrieve remote assets from.
     *
     * @return See short description.
     */
    static QUrl assetUrl();

    /**
     * @brief Returns the maximum number of sides dice can have.
     *
     * @return See short description.
     */
    static int diceMaxValue();

    /**
     * @brief Returns the maximum number of dice that can be rolled at once.
     *
     * @return See short description.
     */
    static int diceMaxDice();

    /**
     * @brief Returns true if the discord webhook is enabled.
     *
     * @return See short description.
     */
    static bool discordWebhookEnabled();

    /**
     * @brief Returns the discord webhook URL.
     *
     * @return See short description.
     */
    static QString discordWebhookUrl();

    /**
     * @brief Returns the discord webhook content.
     *
     * @return See short description.
     */
    static QString discordWebhookContent();

    /**
     * @brief Returns true if the discord webhook should send log files.
     *
     * @return See short description.
     */
    static bool discordWebhookSendFile();

    /**
     * @brief Returns true if password requirements should be enforced.
     *
     * @return See short description.
     */
    static bool passwordRequirements();

    /**
     * @brief Returns the minimum length passwords must be.
     *
     * @return See short description.
     */
    static int passwordMinLength();

    /**
     * @brief Returns the maximum length passwords can be, or `0` for unlimited length.
     *
     * @return See short description.
     */
    static int passwordMaxLength();

    /**
     * @brief Returns true if passwords must be mixed case.
     *
     * @return See short description.
     */
    static bool passwordRequireMixCase();

    /**
     * @brief Returns true is passwords must contain one or more numbers.
     *
     * @return See short description.
     */
    static bool passwordRequireNumbers();

    /**
     * @brief Returns true if passwords must contain one or more special characters..
     *
     * @return See short description.
     */
    static bool passwordRequireSpecialCharacters();

    /**
     * @brief Returns true if passwords can contain the username.
     *
     * @return See short description.
     */
    static bool passwordCanContainUsername();

    /**
     * @brief Returns the duration before a client is considered AFK.
     *
     * @return See short description.
     */
    static int afkTimeout();

    /**
     * @brief Returns a list of magic 8 ball answers.
     *
     * @return See short description.
     */
    static QStringList magic8BallAnswers();

    /**
     * @brief Returns a list of praises.
     *
     * @return See short description.
     */
    static QStringList praiseList();

    /**
     * @brief Returns a list of reprimands.
     *
     * @return See short description.
     */
    static QStringList reprimandsList();

    /**
     * @brief Returns the server gimp list.
     *
     * @return See short description.
     */
    static QStringList gimpList();

    /**
     * @brief Sets the server's authorization type.
     *
     * @param f_auth The auth type to set.
     */
    static void setAuthType(const DataTypes::AuthType f_auth);

    /**
     * @brief Sets the server's Message of the Day.
     *
     * @param f_motd The MOTD to set.
     */
    static void setMotd(const QString f_motd);

private:
    /**
     * @brief Checks if a file exists and is valid.
     *
     * @param file The file to check.
     *
     * @return True if the file exists and is valid, false otherwise.
     */
    static bool fileExists(const QFileInfo& file);

    /**
     * @brief Checks if a directory exists and is valid.
     *
     * @param file The directory to check.
     *
     * @return True if the directory exists and is valid, false otherwise.
     */
    static bool dirExists(const QFileInfo& dir);

    /**
     * @brief A struct containing the server settings.
     */
    struct server_settings {
        // Options
        bool advertise; //!< Whether the server should advertise to the master server.
        int max_players; //!< Max number of players that can connect at once.
        QString ms_ip; //!< IP of the master server.
        int ms_port; //!< Port of the master server.
        int port; //!< Server port.
        QString server_description; //!< Server description.
        QString server_name; //!< Server name.
        QString motd; //!< Server Message of the Day.
        bool webao_enable; //!< Whether the server should accept WebAO connections.
        int webao_port; //!< Websocket port.
        DataTypes::AuthType auth; //!< Server authorization type.
        QString modpass; //!< Server moderator password.
        int logbuffer; //!< Logbuffer length.
        DataTypes::LogType logging; //!< Server logging type.
        int maximum_statements; //!< Max testimony recorder statements.
        int multiclient_limit; //!< Max number of multiclient connections.
        int maximum_characters; //!< Max characters in a message.
        int message_floodguard; //!< Message floodguard length.
        QUrl asset_url; //!< Server asset URL.
        int afk_timeout; //!< Server AFK timeout length.
        // Dice
        int max_value; //!< Max dice sides.
        int max_dice; //!< Max amount of dice.
        // Discord
        bool webhook_enabled; //!< Whether the Discord webhook is enabled.
        QString webhook_url; //!< URL of the Discord webhook.
        QString webhook_content; //!< The content to send to the Discord webhook.
        bool webhook_sendfile; //!< Whether to send log files to the Discord webhook.
        // Password
        bool password_requirements; //!< Whether to enforce password requirements.
        int pass_min_length; //!< Minimum length of passwords.
        int pass_max_length; //!< Maximum length of passwords.
        bool pass_required_mix_case; //!< Whether passwords require mixed case.
        bool pass_required_numbers; //!< Whether passwords require numbers.
        bool pass_required_special; //!< Whether passwords require special characters.
        bool pass_can_contain_username; //!< Whether passwords can contain the username.
        // config/text/
        QStringList magic_8ball_answers; //!< List of 8ball answers, from 8ball.txt
        QStringList praise_list; //!< List of praises, from praise.txt
        QStringList reprimands_list; //!< List of reprimands, from reprimands.txt
        QStringList gimp_list; //!< List of gimp phrases, from gimp.txt
    };

    /**
     * @brief Stores all server configuration values.
     */
    static server_settings* m_settings;

    /**
     * @brief Returns a stringlist with the contents of a .txt file from config/text/.
     *
     * @param Name of the file to load.
     */
    static QStringList loadConfigFile(const QString filename);
};



#endif // CONFIG_MANAGER_H
