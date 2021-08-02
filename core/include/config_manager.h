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
     * @brief Returns true if the discord ban webhook is enabled.
     *
     * @return See short description.
     */
    static bool discordBanWebhookEnabled();

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
     * @brief Returns if the HTTP advertiser is constructed or not.
     */
    static bool advertiseHTTPServer();

    /**
     * @brief Returns if the HTTP advertiser prints debug info to console.
     */
    static bool advertiserHTTPDebug();

    /**
     * @brief Returns the IP or URL of the masterserver.
     */
    static QUrl advertiserHTTPIP();


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

    /**
     * @brief Reload the server configuration.
     */
    static void reloadSettings();

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
     * @brief A struct for storing QStringLists loaded from command configuration files.
     */
    struct CommandSettings {
        QStringList magic_8ball; //!< Contains answers for /8ball, found in config/text/8ball.txt
        QStringList praises; //!< Contains command praises, found in config/text/praises.txt
        QStringList reprimands; //!< Contains command reprimands, found in config/text/reprimands.txt
        QStringList gimps; //!< Contains phrases for /gimp, found in config/text/gimp.txt
    };

    /**
     * @brief Contains the settings required for various commands.
     */
    static CommandSettings* m_commands;

    /**
     * @brief Stores all server configuration values.
     */
    static QSettings* m_settings;

    /**
     * @brief Returns a stringlist with the contents of a .txt file from config/text/.
     *
     * @param Name of the file to load.
     */
    static QStringList loadConfigFile(const QString filename);
};



#endif // CONFIG_MANAGER_H
