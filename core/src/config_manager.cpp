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

#include <include/config_manager.h>

QSettings* ConfigManager::m_settings = new QSettings("config/config.ini", QSettings::IniFormat);
QSettings* ConfigManager::m_discord = new QSettings("config/discord.ini", QSettings::IniFormat);
ConfigManager::CommandSettings* ConfigManager::m_commands = new CommandSettings();
QElapsedTimer* ConfigManager::m_uptimeTimer = new QElapsedTimer;

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

    // Verify config settings
    m_settings->beginGroup("Options");
    bool ok;
    m_settings->value("ms_port", 27016).toInt(&ok);
    if (!ok) {
        qCritical("ms_port is not a valid port!");
        return false;
    }
    m_settings->value("port", 27016).toInt(&ok);
    if (!ok) {
        qCritical("port is not a valid port!");
        return false;
    }
    bool web_ao = m_settings->value("webao_enable", false).toBool();
    if (!web_ao) {
        m_settings->setValue("webao_port", -1);
    }
    else {
        m_settings->value("webao_port", 27017).toInt(&ok);
        if (!ok) {
            qCritical("webao_port is not a valid port!");
            return false;
        }
    }
    QString l_auth = m_settings->value("auth", "simple").toString().toLower();
    if (!(l_auth == "simple" || l_auth == "advanced")) {
        qCritical("auth is not a valid auth type!");
        return false;
    }
    m_settings->endGroup();
    m_commands->magic_8ball = (loadConfigFile("8ball"));
    m_commands->praises = (loadConfigFile("praise"));
    m_commands->reprimands = (loadConfigFile("reprimands"));
    m_commands->gimps = (loadConfigFile("gimp"));

    m_uptimeTimer->start();

    return true;
}

void ConfigManager::reloadSettings()
{
    m_settings->sync();
    m_discord->sync();
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
    return m_settings->value("Options/advertise", true).toBool();
}

int ConfigManager::maxPlayers()
{
    bool ok;
    int l_players = m_settings->value("Options/max_players", 100).toInt(&ok);
    if (!ok) {
        qWarning("max_players is not an int!");
        l_players = 100;
    }
    return l_players;
}

QString ConfigManager::masterServerIP()
{
    return m_settings->value("Options/ms_ip", "master.aceattorneyonline.com").toString();
}

int ConfigManager::masterServerPort()
{
    return m_settings->value("Options/ms_port", 27016).toInt();
}

int ConfigManager::serverPort()
{
    return m_settings->value("Options/port", 27016).toInt();
}

QString ConfigManager::serverDescription()
{
    return m_settings->value("Options/server_description", "This is my flashy new server!").toString();
}

QString ConfigManager::serverName()
{
    return m_settings->value("Options/server_name", "An Unnamed Server").toString();
}

QString ConfigManager::motd()
{
    return m_settings->value("Options/motd", "MOTD not set").toString();
}

bool ConfigManager::webaoEnabled()
{
    return m_settings->value("Options/webao_enable", false).toBool();
}

int ConfigManager::webaoPort()
{
    return m_settings->value("Options/webao_port", 27017).toInt();
}

DataTypes::AuthType ConfigManager::authType()
{
    QString l_auth = m_settings->value("Options/auth", "simple").toString().toUpper();
    return toDataType<DataTypes::AuthType>(l_auth);
}

QString ConfigManager::modpass()
{
    return m_settings->value("Options/modpass", "changeme").toString();
}

int ConfigManager::logBuffer()
{
    bool ok;
    int l_buffer = m_settings->value("Options/logbuffer", 500).toInt(&ok);
    if (!ok) {
        qWarning("logbuffer is not an int!");
        l_buffer = 500;
    }
    return l_buffer;
}

DataTypes::LogType ConfigManager::loggingType()
{
    QString l_log = m_settings->value("Options/logging", "modcall").toString().toUpper();
    return toDataType<DataTypes::LogType>(l_log);
}

int ConfigManager::maxStatements()
{
    bool ok;
    int l_max = m_settings->value("Options/maximum_statements", 10).toInt(&ok);
    if (!ok) {
        qWarning("maximum_statements is not an int!");
        l_max = 10;
    }
    return l_max;
}
int ConfigManager::multiClientLimit()
{
    bool ok;
    int l_limit = m_settings->value("Options/multiclient_limit", 15).toInt(&ok);
    if (!ok) {
        qWarning("multiclient_limit is not an int!");
        l_limit = 15;
    }
    return l_limit;
}

int ConfigManager::maxCharacters()
{
    bool ok;
    int l_max = m_settings->value("Options/maximum_characters", 256).toInt(&ok);
    if (!ok) {
        qWarning("maximum_characters is not an int!");
        l_max = 256;
    }
    return l_max;
}

int ConfigManager::messageFloodguard()
{
    bool ok;
    int l_flood = m_settings->value("Options/message_floodguard", 250).toInt(&ok);
    if (!ok) {
        qWarning("message_floodguard is not an int!");
        l_flood = 250;
    }
    return l_flood;
}

QUrl ConfigManager::assetUrl()
{
    QByteArray l_url = m_settings->value("Options/asset_url", "").toString().toUtf8();
    if (QUrl(l_url).isValid()) {
        return QUrl(l_url);
    }
    else {
        qWarning("asset_url is not a valid url!");
        return QUrl(NULL);
    }
}

int ConfigManager::diceMaxValue()
{
    bool ok;
    int l_value = m_settings->value("Dice/max_value", 100).toInt(&ok);
    if (!ok) {
        qWarning("max_value is not an int!");
        l_value = 100;
    }
    return l_value;
}

int ConfigManager::diceMaxDice()
{
    bool ok;
    int l_dice = m_settings->value("Dice/max_dice", 100).toInt(&ok);
    if (!ok) {
        qWarning("max_dice is not an int!");
        l_dice = 100;
    }
    return l_dice;
}

bool ConfigManager::discordWebhookEnabled()
{
    return m_discord->value("Discord/webhook_enabled", false).toBool();
}

bool ConfigManager::discordModcallWebhookEnabled()
{
    return m_discord->value("Discord/webhook_modcall_enabled", false).toBool();
}

QString ConfigManager::discordModcallWebhookUrl()
{
    return m_discord->value("Discord/webhook_modcall_url", "").toString();
}

QString ConfigManager::discordModcallWebhookContent()
{
    return m_discord->value("Discord/webhook_modcall_content", "").toString();
}

bool ConfigManager::discordModcallWebhookSendFile()
{
    return m_discord->value("Discord/webhook_modcall_sendfile", false).toBool();
}

bool ConfigManager::discordBanWebhookEnabled()
{
    return m_discord->value("Discord/webhook_ban_enabled", false).toBool();
}

QString ConfigManager::discordBanWebhookUrl()
{
    return m_discord->value("Discord/webhook_ban_url", "").toString();
}

bool ConfigManager::discordUptimeEnabled()
{
    return m_discord->value("Discord/webhook_uptime_enabled","false").toBool();
}

int ConfigManager::discordUptimeTime()
{
    bool ok;
    int l_aliveTime = m_discord->value("Discord/webhook_uptime_time","60").toInt(&ok);
    if (!ok) {
        qWarning("alive_time is not an int");
        l_aliveTime = 60;
    }
    return l_aliveTime;
}

QString ConfigManager::discordUptimeWebhookUrl()
{
    return m_discord->value("Discord/webhook_uptime_url", "").toString();
}

bool ConfigManager::passwordRequirements()
{
    return m_settings->value("Password/password_requirements", true).toBool();
}

int ConfigManager::passwordMinLength()
{
    bool ok;
    int l_min = m_settings->value("Password/pass_min_length", 8).toInt(&ok);
    if (!ok) {
        qWarning("pass_min_length is not an int!");
        l_min = 8;
    }
    return l_min;
}

int ConfigManager::passwordMaxLength()
{
    bool ok;
    int l_max = m_settings->value("Password/pass_max_length", 0).toInt(&ok);
    if (!ok) {
        qWarning("pass_max_length is not an int!");
        l_max = 0;
    }
    return l_max;
}

bool ConfigManager::passwordRequireMixCase()
{
    return m_settings->value("Password/pass_required_mix_case", true).toBool();
}

bool ConfigManager::passwordRequireNumbers()
{
    return m_settings->value("Password/pass_required_numbers", true).toBool();
}

bool ConfigManager::passwordRequireSpecialCharacters()
{
    return m_settings->value("Password/pass_required_special", true).toBool();
}

bool ConfigManager::passwordCanContainUsername()
{
    return m_settings->value("Password/pass_can_contain_username", false).toBool();
}

int ConfigManager::afkTimeout()
{
    bool ok;
    int l_afk = m_settings->value("Options/afk_timeout", 300).toInt(&ok);
    if (!ok) {
        qWarning("afk_timeout is not an int!");
        l_afk = 300;
    }
    return l_afk;
}

void ConfigManager::setAuthType(const DataTypes::AuthType f_auth)
{
    m_settings->setValue("Options/auth", fromDataType<DataTypes::AuthType>(f_auth).toLower());
}

QStringList ConfigManager::magic8BallAnswers()
{
    return m_commands->magic_8ball;
}

QStringList ConfigManager::praiseList()
{
    return m_commands->praises;
}

QStringList ConfigManager::reprimandsList()
{
    return m_commands->reprimands;
}

QStringList ConfigManager::gimpList()
{
    return m_commands->gimps;
}

bool ConfigManager::advertiseHTTPServer()
{
    return m_settings->value("ModernAdvertiser/advertise","true").toBool();
}

bool ConfigManager::advertiserHTTPDebug()
{
    return m_settings->value("ModernAdvertiser/debug","true").toBool();
}

QUrl ConfigManager::advertiserHTTPIP()
{
    return m_settings->value("ModernAdvertiser/ms_ip","").toUrl();
}

qint64 ConfigManager::uptime()
{
    return m_uptimeTimer->elapsed();
}

void ConfigManager::setMotd(const QString f_motd)
{
    m_settings->setValue("Options/motd", f_motd);
}

bool ConfigManager::fileExists(const QFileInfo &f_file)
{
    return (f_file.exists() && f_file.isFile());
}

bool ConfigManager::dirExists(const QFileInfo &f_dir)
{
    return (f_dir.exists() && f_dir.isDir());
}
