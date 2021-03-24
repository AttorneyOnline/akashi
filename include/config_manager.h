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

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>

/**
 * @brief The config file handler class.
 */
class ConfigManager {
  public:
    /**
     * @brief An empty constructor for ConfigManager.
     */
    ConfigManager() {};

    /**
     * @brief Performs some preliminary checks for the various configuration files used by the server.
     *
     * @return True if the config file exists, is up-to-date, and valid, false otherwise.
     */
    bool initConfig();

    /**
     * @brief Updates the config file's version to the one used by the server currently.
     *
     * @details The function can return false if the server's expected config version is 0
     * (doesn't actually exist), or negative (nonsense).
     * If the current config file lags more than one version behind the expected, all intermediate
     * updates are also performed on the config file.
     *
     * @param current_version The current configuration version expected by the server.
     *
     * @return True if a version update took place, false otherwise.
     */
    bool updateConfig(int current_version);

    /**
     * @brief The collection of server-specific settings.
     */
    struct server_settings {
        QString ms_ip; //!< The IP address of the master server to establish connection to.
        int port; //!< The TCP port the server will accept client connections through.
        int ws_port; //!< The WebSocket port the server will accept client connections through.
        int ms_port; //!< The port of the master server to establish connection to.
        QString name; //!< The name of the server as advertised on the server browser.
        QString description; //!< The description of the server as advertised on the server browser.
        bool advertise_server; //!< The server will only be announced to the master server (and thus appear on the master server list) if this is true.
        int zalgo_tolerance; //!< The amount of subscripts zalgo is stripped by.
    };

    /**
     * @brief Loads the server settings into the given struct from the config file.
     *
     * @param[out] settings Pointer to a server_settings file to be filled with data.
     *
     * @return False if any of the ports (the master server connection port,
     * the TCP port used by clients, or the WebSocket port used by WebAO) failed
     * to be read in from the settings correctly, true otherwise.
     *
     * @pre initConfig() must have been run beforehand to check for the config file's existence.
     */
    bool loadServerSettings(server_settings* settings);

  private:
    /**
     * @brief Convenience function to check if the object exists, and is a file.
     *
     * @param file The object to check.
     *
     * @return See brief description.
     */
    bool fileExists(QFileInfo *file);
};

#endif // CONFIG_MANAGER_H
