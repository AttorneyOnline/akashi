#include <include/config_manager.h>

ConfigManager::ConfigManager(QSettings* p_config)
{
    config = p_config;
}

// Validate and set up the config
bool ConfigManager::initConfig()
{
    config->beginGroup("Info");
        QString config_version = config->value("version", "none").toString();
    config->endGroup();
    if(config_version == "none") {
        QFileInfo check_file("config.ini");
        // TODO: put proper translatable warnings here
        if (!(check_file.exists() && check_file.isFile())){
            // TODO: send signal config doesn't exist
            generateDefaultConfig(false);
        } else {
            // TODO: send signal config is invalid
            generateDefaultConfig(true);
        }
        return false;
    }
    else if(config_version != QString::number(CONFIG_VERSION)) {
        bool version_number_is_valid;
        int current_version = config_version.toInt(&version_number_is_valid);
        if(version_number_is_valid)
            updateConfig(current_version);
        else
            generateDefaultConfig(true); // Version number isn't a number at all
                                         // This means the config is invalid
                                         // TODO: send invalid config signal
        return false;
    } else {
        // Config is valid and up to date, so let's go ahead
        return true;
    }
}

// Setting backup_old to true will move the existing config.ini to config_old.ini
void ConfigManager::generateDefaultConfig(bool backup_old)
{
    qDebug() << "Config is invalid or missing, making a new one...";
    QDir dir = QDir::current();
    if(backup_old) {
        // TODO: failsafe if config_old.ini already exists
        dir.rename("config.ini", "config_old.ini");
    }

    // Group: Info
    // This contains basic metadata about the config
    config->beginGroup("Info");
        config->setValue("version", CONFIG_VERSION);
    config->endGroup();

    // Group: Options
    // This contains general configuration
    config->beginGroup("Options");
        config->setValue("language", "en");
        config->setValue("hostname", "$H");
        config->setValue("max_players", "100");
        config->setValue("port", "27016");
        config->setValue("webao_enable", "true");
        config->setValue("webao_port", "27017");
        config->setValue("modpass", "password");
        config->setValue("advertise", "true");
        config->setValue("ms_ip", "master.aceattorneyonline.com");
        config->setValue("ms_port", "27016");
        config->setValue("server_name", "My First Server");
        config->setValue("server_description", "This is my flashy new server");
        config->setValue("multiclient_limit", "16");
        config->setValue("max_message_size", "256");
    config->endGroup();
}

// Ensure version continuity with config versions
void ConfigManager::updateConfig(int current_version)
{
    if(current_version > CONFIG_VERSION) {
        // Config version is newer than the latest version, and the config is invalid
        // This could also mean the server is out of date, and the user should be shown a relevant message
        // Regardless, regen the config anyways
        // TODO: send signal config is invalid
        generateDefaultConfig(true);
    }
    else if (current_version < 0){
        // Negative version number? Invalid!
        generateDefaultConfig(true);
    } else {
        // TODO: send signal config is out of date, and is being updated
        // Update the config as needed using a switch. This is nice because we can fall through as we go up the version ladder.
        switch(current_version){
        case 0: // Version 0 doesn't actually exist, but we should check for it just in case
        case 1:
            config->beginGroup("Info");
                config->setValue("version", CONFIG_VERSION);
            config->endGroup();
            break; // This is the newest version, and nothing more needs to be done
        }
    }
}

// Validate and retriever settings related to advertising and the server
bool ConfigManager::loadAdvertiserSettings(QString* ms_ip, int* port, int* ws_port, int* local_port, QString* name, QString* description, bool* advertise_server)
{
    // TODO: Move this logic into config_manager.cpp
    bool port_conversion_success;
    bool ws_port_conversion_success;
    bool local_port_conversion_success;
    config->beginGroup("Options");
        *ms_ip = config->value("ms_ip", "master.aceattorneyonline.com").toString();
        *port = config->value("ms_port", "27016").toInt(&port_conversion_success);
        *ws_port = config->value("webao_port", "27017").toInt(&ws_port_conversion_success);
        *local_port = config->value("port", "27016").toInt(&local_port_conversion_success);
        *name = config->value("server_name", "My First Server").toString();
        *description = config->value("server_description", "This is my flashy new server").toString();
    config->endGroup();
    if(!port_conversion_success || !ws_port_conversion_success || !local_port_conversion_success) {
        return false;
    } else {
        if(config->value("advertise", "true").toString() != "true")
            *advertise_server = false;
        else
            *advertise_server = true;

        if(config->value("webao_enable", "true").toString() != "true")
            *ws_port = -1;

        return true;
    }
}
