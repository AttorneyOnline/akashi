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

ConfigManager::server_settings* ConfigManager::m_settings = new server_settings();

bool ConfigManager::verifyServerConfig()
{
    // Verify directories
    QStringList l_directories{"config/", "config/text/"};
    for (QString l_directory : l_directories) {
        if (!dirExists(QFileInfo(l_directory))) {
                qCritical() << l_directory + " does not exist!";
                return false;
        }
    }

    // Verify config files
    QStringList l_config_files{"config/config.ini", "config/areas.ini", "config/backgrounds.txt", "config/characters.txt", "config/music.txt",
                              "config/text/8ball.txt", "config/text/gimp.txt", "config/text/praise.txt", "config/text/reprimands.txt"};
    for (QString l_file : l_config_files) {
        if (!fileExists(QFileInfo(l_file))) {
            qCritical() << l_file + " does not exist!";
            return false;
        }
    }

    // Verify areas
    QSettings l_areas_ini("config/areas.ini", QSettings::IniFormat);
    l_areas_ini.setIniCodec("UTF-8");
    if (l_areas_ini.childGroups().length() < 1) {
        qCritical() << "areas.ini is invalid!";
        return false;
    }
    return true;
}

bool ConfigManager::loadConfigSettings()
{
    QSettings l_config("config/config.ini", QSettings::IniFormat);
    l_config.setIniCodec("UTF-8");
    l_config.beginGroup("Options");
    bool ok;
    m_settings->ms_port = l_config.value("ms_port", "27016").toInt(&ok);
    if (!ok) {
        qCritical("ms_port is not a valid port!");
        return false;
    }
    m_settings->port = l_config.value("port", "27016").toInt(&ok);
    if (!ok) {
        qCritical("port is not a valid port!");
        return false;
    }
    m_settings->webao_enable = l_config.value("webao_enable", "true").toBool();
    if (!m_settings->webao_enable) {
        m_settings->webao_port = -1;
    }
    else {
        m_settings->webao_port = l_config.value("webao_port", "27017").toInt(&ok);
        if (!ok) {
            qCritical("webao_port is not a valid port!");
            return false;
        }
    }
    m_settings->ms_ip = l_config.value("ms_ip", "master.aceattorneyonline.com").toString();
    m_settings->advertise = l_config.value("advertise", "true").toBool();
    l_config.endGroup();

    if (reloadConfigSettings())
        return true;
    else
        return false;
}

bool ConfigManager::reloadConfigSettings()
{
    QSettings l_config("config/config.ini", QSettings::IniFormat);
    l_config.setIniCodec("UTF-8");
    l_config.beginGroup("Options");
    bool ok;

    // Options
    m_settings->max_players = l_config.value("max_players", "100").toInt(&ok);
    if (!ok) {
        qWarning("max_players is not an int!");
        m_settings->max_players = 100;
    }
    m_settings->server_description = l_config.value("server_description", "This is my flashy new server!").toString();
    m_settings->server_name = l_config.value("server_name", "An Unnamed Server").toString();
    m_settings->motd = l_config.value("motd", "MOTD not set").toString();
    QString l_auth = l_config.value("auth", "simple").toString();
    if (l_auth == "simple" || l_auth == "advanced") {
        m_settings->auth = toDataType<DataTypes::AuthType>(l_auth);
    }
    else {
        qCritical("auth is not a valid auth type!");
        return false;
    }
    m_settings->modpass = l_config.value("modpass", "changeme").toString();
    m_settings->logbuffer = l_config.value("logbuffer", "500").toInt(&ok);
    if (!ok) {
        qWarning("logbuffer is not an int!");
        m_settings->logbuffer = 500;
    }
    QString l_log = l_config.value("logging", "modcall").toString();
    if (l_log == "modcall" || l_log == "full") {
        m_settings->logging = toDataType<DataTypes::LogType>(l_log);
    }
    else {
        qWarning("logging is not a valid log type!");
        m_settings->logging = DataTypes::LogType::MODCALL;
    }
    m_settings->maximum_statements = l_config.value("maximum_statements", "10").toInt(&ok);
    if (!ok) {
        qWarning("maximum_statements is not an int!");
        m_settings->maximum_statements = 10;
    }
    m_settings->multiclient_limit = l_config.value("multiclient_limit", "15").toInt(&ok);
    if (!ok) {
        qWarning("multiclient_limit is not an int!");
        m_settings->multiclient_limit = 15;
    }
    m_settings->maximum_characters = l_config.value("maximum_characters", "256").toInt(&ok);
    if (!ok) {
        qWarning("maximum_characters is not an int!");
        m_settings->maximum_characters = 256;
    }
    m_settings->message_floodguard = l_config.value("message_floodguard", "250").toInt(&ok);
    if (!ok) {
        qWarning("message_floodguard is not an in!");
        m_settings->message_floodguard = 250;
    }
    m_settings->asset_url = l_config.value("asset_url", "").toString().toUtf8();
    if (!m_settings->asset_url.isValid()) {
        qWarning("asset_url is not a valid url!");
        m_settings->asset_url = NULL;
    }
    l_config.endGroup();

    // Dice
    l_config.beginGroup("Dice");
    m_settings->max_value = l_config.value("max_value", "100").toInt(&ok);
    if (!ok) {
        qWarning("max_value is not an int!");
        m_settings->max_value = 100;
    }
    m_settings->max_dice = l_config.value("max_dice", "100").toInt(&ok);
    if (!ok) {
        qWarning("max_dice is not an int!");
        m_settings->max_dice = 100;
    }
    l_config.endGroup();

    // Discord
    l_config.beginGroup("Discord");
    m_settings->webhook_enabled = l_config.value("webhook_enabled", "false").toBool();
    m_settings->webhook_url = l_config.value("webhook_url", "Your webhook url here.").toString();
    m_settings->webhook_sendfile = l_config.value("webhook_sendfile", "false").toBool();
    m_settings->webhook_content = l_config.value("webhook_content", "").toString();
    l_config.endGroup();

    // Password
    l_config.beginGroup("Password");
    m_settings->password_requirements = l_config.value("password_requirements", "true").toBool();
    m_settings->pass_min_length = l_config.value("pass_min_length", "8").toInt(&ok);
    if (!ok) {
        qWarning("pass_min_length is not an int!");
        m_settings->pass_min_length = 8;
    }
    m_settings->pass_max_length = l_config.value("pass_max_length", "0").toInt(&ok);
    if (!ok) {
        qWarning("pass_max_length is not an int!");
        m_settings->pass_max_length = 0;
    }
    m_settings->pass_required_mix_case = l_config.value("pass_required_mix_case", "true").toBool();
    m_settings->pass_required_numbers = l_config.value("pass_required_numbers", "true").toBool();
    m_settings->pass_required_special = l_config.value("pass_required_special", "true").toBool();
    m_settings->pass_can_contain_username = l_config.value("pass_can_contain_username", "false").toBool();

    m_settings->afk_timeout = l_config.value("afk_timeout", "300").toInt(&ok);
    if (!ok) {
        qWarning("afk_timeout is not an int!");
        m_settings->afk_timeout = 300;
    }
    l_config.endGroup();

    m_settings->magic_8ball_answers = (loadConfigFile("8ball"));
    m_settings->praise_list = (loadConfigFile("praise"));
    m_settings->reprimands_list = (loadConfigFile("reprimands"));
    m_settings->gimp_list = (loadConfigFile("gimp"));

    return true;
}

QStringList ConfigManager::loadConfigFile(const QString filename)
{
    QStringList stringlist;
    QFile file("config/text/" + filename + ".txt");
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    while (!(file.atEnd())) {
        stringlist.append(file.readLine().trimmed());
    }
    file.close();
    return stringlist;
}

bool ConfigManager::advertiseServer()
{
    return m_settings->advertise;
}

int ConfigManager::maxPlayers()
{
    return m_settings->max_players;
}

QString ConfigManager::masterServerIP()
{
    return m_settings->ms_ip;
}

int ConfigManager::masterServerPort()
{
    return m_settings->ms_port;
}

int ConfigManager::serverPort()
{
    return m_settings->port;
}

QString ConfigManager::serverDescription()
{
    return m_settings->server_description;
}

QString ConfigManager::serverName()
{
    return m_settings->server_name;
}

QString ConfigManager::motd()
{
    return m_settings->motd;
}

bool ConfigManager::webaoEnabled()
{
    return m_settings->webao_enable;
}

int ConfigManager::webaoPort()
{
    return m_settings->webao_port;
}

DataTypes::AuthType ConfigManager::authType()
{
    return m_settings->auth;
}

QString ConfigManager::modpass()
{
    return m_settings->modpass;
}

int ConfigManager::logBuffer()
{
    return m_settings->logbuffer;
}

DataTypes::LogType ConfigManager::loggingType()
{
    return m_settings->logging;
}

int ConfigManager::maxStatements()
{
    return m_settings->maximum_statements;
}
int ConfigManager::multiClientLimit()
{
    return m_settings->multiclient_limit;
}

int ConfigManager::maxCharacters()
{
    return m_settings->maximum_characters;
}

int ConfigManager::messageFloodguard()
{
    return m_settings->message_floodguard;
}

QUrl ConfigManager::assetUrl()
{
    return m_settings->asset_url;
}

int ConfigManager::diceMaxValue()
{
    return m_settings->max_value;
}

int ConfigManager::diceMaxDice()
{
    return m_settings->max_dice;
}

bool ConfigManager::discordWebhookEnabled()
{
    return m_settings->webao_enable;
}

QString ConfigManager::discordWebhookUrl()
{
    return m_settings->webhook_url;
}

QString ConfigManager::discordWebhookContent()
{
    return m_settings->webhook_content;
}

bool ConfigManager::discordWebhookSendFile()
{
    return m_settings->webhook_sendfile;
}

bool ConfigManager::passwordRequirements()
{
    return m_settings->password_requirements;
}

int ConfigManager::passwordMinLength()
{
    return m_settings->pass_min_length;
}

int ConfigManager::passwordMaxLength()
{
    return m_settings->pass_max_length;
}

bool ConfigManager::passwordRequireMixCase()
{
    return m_settings->pass_required_mix_case;
}

bool ConfigManager::passwordRequireNumbers()
{
    return m_settings->pass_required_numbers;
}

bool ConfigManager::passwordRequireSpecialCharacters()
{
    return m_settings->pass_required_special;
}

bool ConfigManager::passwordCanContainUsername()
{
    return m_settings->pass_can_contain_username;
}

int ConfigManager::afkTimeout()
{
    return m_settings->afk_timeout;
}

void ConfigManager::setAuthType(const DataTypes::AuthType f_auth)
{
    QSettings l_config("config/config.ini", QSettings::IniFormat);
    l_config.setIniCodec("UTF-8");
    l_config.beginGroup("Options");
    l_config.setValue("auth", fromDataType<DataTypes::AuthType>(f_auth).toLower());

    m_settings->auth = f_auth;
}

QStringList ConfigManager::magic8BallAnswers()
{
    return m_settings->magic_8ball_answers;
}

QStringList ConfigManager::praiseList()
{
    return m_settings->praise_list;
}

QStringList ConfigManager::reprimandsList()
{
    return m_settings->reprimands_list;
}

QStringList ConfigManager::gimpList()
{
    return m_settings->gimp_list;
}

void ConfigManager::setMotd(const QString f_motd)
{
    m_settings->motd = f_motd;
}

bool ConfigManager::fileExists(const QFileInfo &f_file)
{
    return (f_file.exists() && f_file.isFile());
}

bool ConfigManager::dirExists(const QFileInfo &f_dir)
{
    return (f_dir.exists() && f_dir.isDir());
}
