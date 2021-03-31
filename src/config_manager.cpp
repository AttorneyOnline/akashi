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
#include "include/config_manager.h"

// Validate and set up the config
bool ConfigManager::initConfig()
{
    QSettings config("config/config.ini", QSettings::IniFormat);
    QFileInfo config_dir_info("config/");
    if (!config_dir_info.exists() || !config_dir_info.isDir()) {
        qCritical() << "Config directory doesn't exist!";
        return false;
    }

    // Check that areas, characters, and music lists all exist
    QFileInfo areas_info("config/areas.ini");
    QFileInfo characters_info("config/characters.txt");
    QFileInfo music_info("config/music.txt");
    QFileInfo backgrounds_info("config/backgrounds.txt");

    if (!fileExists(&areas_info)) {
        qCritical() << "areas.ini doesn't exist!";
        return false;
    }
    else {
        QSettings areas_ini("config/areas.ini", QSettings::IniFormat);
        if (areas_ini.childGroups().length() < 1) {
            qCritical() << "areas.ini is invalid!";
            return false;
        }
    }
    if (!fileExists(&characters_info)) {
        qCritical() << "characters.txt doesn't exist!";
        return false;
    }
    if (!fileExists(&music_info)) {
        qCritical() << "music.txt doesn't exist!";
        return false;
    }
    if (!fileExists(&backgrounds_info)) {
        qCritical() << "backgrounds.txt doesn't exist!";
        return false;
    }

    config.beginGroup("Info");
    QString config_version = config.value("version", "none").toString();
    config.endGroup();
    if (config_version == "none") {
        QFileInfo check_file("config/config.ini");
        if (!fileExists(&check_file)) {
            qCritical() << "config.ini doesn't exist!";
        }
        else {
            qCritical() << "config.ini is invalid!";
        }
        return false;
    }
    else if (config_version != QString::number(CONFIG_VERSION)) {
        bool version_number_is_valid;
        int current_version = config_version.toInt(&version_number_is_valid);
        if (version_number_is_valid) {
            if (updateConfig(current_version))
                qWarning() << "config.ini was out of date, and has been updated. Please review the changes, and restart the server.";
            else
                qCritical() << "config.ini is invalid!";
        }
        else
            qCritical() << "config.ini is invalid!"; // Version number isn't a number at all
                                                     // This means the config is invalid
        return false;
    }
    else {
        // Config is valid and up to date, so let's go ahead
        return true;
    }
}

// Ensure version continuity with config versions
bool ConfigManager::updateConfig(int current_version)
{
    QSettings config("config/config.ini", QSettings::IniFormat);
    if (current_version > CONFIG_VERSION) {
        // Config version is newer than the latest version, and the config is
        // invalid This could also mean the server is out of date, and the user
        // should be shown a relevant message Regardless, regen the config
        // anyways
        return false;
    }
    else if (current_version < 0) {
        // Negative version number? Invalid!
        return false;
    }
    else {
        // Update the config as needed using a switch. This is nice because we
        // can fall through as we go up the version ladder.
        switch (current_version) {
        case 0: // Version 0 doesn't actually exist, but we should check for it
                // just in case
        case 1:
            config.beginGroup("Info");
            config.setValue("version", CONFIG_VERSION);
            config.endGroup();
            break; // This is the newest version, and nothing more needs to be
                   // done
        }
        return true;
    }
}

// Validate and retriever settings related to advertising and the server
bool ConfigManager::loadServerSettings(server_settings* settings)
{
    QSettings config("config/config.ini", QSettings::IniFormat);
    bool port_conversion_success;
    bool ws_port_conversion_success;
    bool local_port_conversion_success;
    bool zalgo_tolerance_conversion_success;
    config.beginGroup("Options");
    settings->ms_ip =
        config.value("ms_ip", "master.aceattorneyonline.com").toString();
    settings->port =
        config.value("port", "27016").toInt(&port_conversion_success);
    settings->ws_port =
        config.value("webao_port", "27017").toInt(&ws_port_conversion_success);
    settings->ms_port =
        config.value("ms_port", "27016").toInt(&local_port_conversion_success);
    settings->name = config.value("server_name", "My First Server").toString();
    settings->description =
        config.value("server_description", "This is my flashy new server")
            .toString();
    settings->zalgo_tolerance =
        config.value("zalgo_tolerance", "3").toInt(&zalgo_tolerance_conversion_success);
    config.endGroup();
    if (!port_conversion_success || !ws_port_conversion_success ||
        !local_port_conversion_success) {
        return false;
    }
    else {
        config.beginGroup("Options");
        // Will be true of false depending on the key
        settings->advertise_server =
            (config.value("advertise", "true").toString() == "true");

        if (config.value("webao_enable", "true").toString() != "true")
            settings->ws_port = -1;
        config.endGroup();

        return true;
    }
}

bool ConfigManager::fileExists(QFileInfo* file)
{
    return (file->exists() && file->isFile());
}
