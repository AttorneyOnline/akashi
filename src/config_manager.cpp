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
#include "config_manager.h"
#include <QSqlDatabase>
#include <QSqlQuery>

QSettings *ConfigManager::m_settings = new QSettings("config/config.ini", QSettings::IniFormat);
QSettings *ConfigManager::m_discord = new QSettings("config/discord.ini", QSettings::IniFormat);
QSettings *ConfigManager::m_areas = new QSettings("config/areas.ini", QSettings::IniFormat);
QSettings *ConfigManager::m_logtext = new QSettings("config/text/logtext.ini", QSettings::IniFormat);
QSettings *ConfigManager::m_ambience = new QSettings("config/ambience.ini", QSettings::IniFormat);
ConfigManager::CommandSettings *ConfigManager::m_commands = new CommandSettings();
QElapsedTimer *ConfigManager::m_uptimeTimer = new QElapsedTimer;
MusicList *ConfigManager::m_musicList = new MusicList;
QHash<QString, ConfigManager::help> *ConfigManager::m_commands_help = new QHash<QString, ConfigManager::help>;
QStringList *ConfigManager::m_ordered_list = new QStringList;

bool ConfigManager::verifyServerConfig()
{
    // Verify directories
    QStringList l_directories{"config/", "config/text/"};
    for (const QString &l_directory : l_directories) {
        if (!dirExists(QFileInfo(l_directory))) {
            qCritical() << l_directory + " does not exist!";
            return false;
        }
    }

    // Verify config files
    QStringList l_config_files{"config/config.ini", "config/areas.ini", "config/backgrounds.txt", "config/characters.txt", "config/music.json",
                               "config/discord.ini", "config/text/8ball.txt", "config/text/gimp.txt", "config/text/praise.txt",
                               "config/text/reprimands.txt", "config/text/commandhelp.json", "config/text/cdns.txt", "config/ipbans.json"};
    for (const QString &l_file : l_config_files) {
        if (!fileExists(QFileInfo(l_file))) {
            qCritical() << l_file + " does not exist!";
            return false;
        }
    }

    // Verify areas
    QSettings l_areas_ini("config/areas.ini", QSettings::IniFormat);
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
    m_commands->cdns = (loadConfigFile("cdns"));
    if (m_commands->cdns.isEmpty())
        m_commands->cdns = QStringList{"cdn.discord.com"};

    m_uptimeTimer->start();

    return true;
}

QString ConfigManager::bindIP()
{
    return m_settings->value("Options/bind_ip", "all").toString();
}

QStringList ConfigManager::charlist()
{
    QStringList l_charlist;
    QFile l_file("config/characters.txt");
    l_file.open(QIODevice::ReadOnly | QIODevice::Text);
    while (!l_file.atEnd()) {
        l_charlist.append(l_file.readLine().trimmed());
    }
    l_file.close();

    return l_charlist;
}

QStringList ConfigManager::backgrounds()
{
    QStringList l_backgrounds;
    QFile l_file("config/backgrounds.txt");
    l_file.open(QIODevice::ReadOnly | QIODevice::Text);
    while (!l_file.atEnd()) {
        l_backgrounds.append(l_file.readLine().trimmed());
    }
    l_file.close();

    return l_backgrounds;
}

MusicList ConfigManager::musiclist()
{
    QFile l_music_json("config/music.json");
    l_music_json.open(QIODevice::ReadOnly | QIODevice::Text);

    QJsonParseError l_error;
    QJsonDocument l_music_list_json = QJsonDocument::fromJson(l_music_json.readAll(), &l_error);
    if (!(l_error.error == QJsonParseError::NoError)) { // Non-Terminating error.
        qWarning() << "Unable to load musiclist. The following error was encounted : " + l_error.errorString();
        return QMap<QString, QPair<QString, int>>{}; // Server can still run without music.
    }

    // Make sure the list is empty before appending new data.
    if (!m_ordered_list->empty()) {
        m_ordered_list->clear();
    }

    // Akashi expects the musiclist to be contained in a JSON array, even if its only a single category.
    QJsonArray l_Json_root_array = l_music_list_json.array();
    QJsonObject l_child_obj;
    QJsonArray l_child_array;

    for (int i = 0; i < l_Json_root_array.size(); i++) { // Iterate trough entire JSON file to assemble musiclist
        l_child_obj = l_Json_root_array.at(i).toObject();

        // Technically not a requirement, but neat for organisation.
        QString l_category_name = l_child_obj["category"].toString();
        if (!l_category_name.isEmpty()) {
            m_musicList->insert(l_category_name, {l_category_name, 0});
            m_ordered_list->append(l_category_name);
        }
        else {
            qWarning() << "Category name not set. This may cause the musiclist to be displayed incorrectly.";
        }

        l_child_array = l_child_obj["songs"].toArray();
        for (int i = 0; i < l_child_array.size(); i++) { // Inner for loop because a category can contain multiple songs.
            QJsonObject l_song_obj = l_child_array.at(i).toObject();
            QString l_song_name = l_song_obj["name"].toString();
            QString l_real_name = l_song_obj["realname"].toString();
            if (l_real_name.isEmpty()) {
                l_real_name = l_song_name;
            }
            int l_song_duration = l_song_obj["length"].toVariant().toInt();
            m_musicList->insert(l_song_name, {l_real_name, l_song_duration});
            m_ordered_list->append(l_song_name);
        }
    }
    l_music_json.close();

    return *m_musicList;
}

QStringList ConfigManager::ordered_songs()
{
    return *m_ordered_list;
}

void ConfigManager::loadCommandHelp()
{
    QFile l_help_json("config/text/commandhelp.json");
    l_help_json.open(QIODevice::ReadOnly | QIODevice::Text);

    QJsonParseError l_error;
    QJsonDocument l_help_list_json = QJsonDocument::fromJson(l_help_json.readAll(), &l_error);
    if (!(l_error.error == QJsonParseError::NoError)) { // Non-Terminating error.
        qWarning() << "Unable to load help information. The following error occurred: " + l_error.errorString();
    }

    // Akashi expects the helpfile to contain multiple entires, so it always checks for an array first.
    QJsonArray l_Json_root_array = l_help_list_json.array();
    QJsonObject l_child_obj;
    QJsonArray l_names;

    for (int i = 0; i < l_Json_root_array.size(); i++) {
        l_child_obj = l_Json_root_array.at(i).toObject();
        l_names = l_child_obj["names"].toArray();
        QString l_usage = l_child_obj["usage"].toString();
        QString l_text = l_child_obj["text"].toString();

        for (int j = 0; j < l_names.size(); j++) {
            QString l_name = l_names.at(j).toString();
            if (!l_name.isEmpty()) {
                help l_help_information = {
                    .usage = l_usage,
                    .text = l_text};

                m_commands_help->insert(l_name, l_help_information);
            }
        }
    }
}

QSettings *ConfigManager::areaData()
{
    return m_areas;
}

QSettings *ConfigManager::ambience()
{
    return m_ambience;
}

QStringList ConfigManager::sanitizedAreaNames()
{
    QStringList l_area_names = m_areas->childGroups(); // invisibly does a lexicographical sort, because Qt is great like that
    std::sort(l_area_names.begin(), l_area_names.end(), [](const QString &a, const QString &b) { return a.split(":")[0].toInt() < b.split(":")[0].toInt(); });
    QStringList l_sanitized_area_names;
    for (const QString &areaName : qAsConst(l_area_names)) {
        QStringList l_nameSplit = areaName.split(":");
        l_nameSplit.removeFirst();
        QString l_area_name_sanitized = l_nameSplit.join(":");
        l_sanitized_area_names.append(l_area_name_sanitized);
    }
    return l_sanitized_area_names;
}

QStringList ConfigManager::rawAreaNames()
{
    return m_areas->childGroups();
}

QStringList ConfigManager::iprangeBans()
{
    QFile l_json_file("config/ipbans.json");
    l_json_file.open(QIODevice::ReadOnly | QIODevice::Text);

    QJsonParseError l_error;
    QJsonDocument l_ip_bans = QJsonDocument::fromJson(l_json_file.readAll(), &l_error);
    if (l_error.error != QJsonParseError::NoError) {
        qDebug() << "Unable to parse JSON file. Error:" << l_error.errorString();
        return {};
    }

    QJsonObject l_json_obj = l_ip_bans.object();

    QStringList l_range_bans;
    l_range_bans.append(l_json_obj["ip_range"].toVariant().toStringList());

    if (QFile::exists("storage/asn.sqlite3")) {
        QSqlDatabase asn_db = QSqlDatabase::addDatabase("QSQLITE", "ASN");
        asn_db.setDatabaseName("storage/asn.sqlite3");
        asn_db.open();

        // This is a dumb hack. Idk how else I can do this, but who gives a shit?
        QSqlQuery query("SELECT ip FROM maxmind WHERE asn in (" + l_json_obj["asn"].toVariant().toStringList().join(",") + ")", asn_db);
        query.exec();
        while (query.next()) {
            l_range_bans.append(query.value(0).toString());
        }
        asn_db.close();
    }
    l_range_bans.removeDuplicates();
    return l_range_bans;
}

void ConfigManager::reloadSettings()
{
    m_settings->sync();
    m_discord->sync();
    m_logtext->sync();
}

QStringList ConfigManager::loadConfigFile(const QString filename)
{
    QStringList stringlist;
    QFile l_file("config/text/" + filename + ".txt");
    l_file.open(QIODevice::ReadOnly | QIODevice::Text);
    while (!(l_file.atEnd())) {
        stringlist.append(l_file.readLine().trimmed());
    }
    l_file.close();
    return stringlist;
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

int ConfigManager::serverPort()
{
    if (m_settings->contains("Options/webao_port")) {
        qWarning("webao_port is deprecated, use port instead");
        return m_settings->value("Options/webao_port", 27016).toInt();
    }

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

int ConfigManager::globalMessageFloodguard()
{
    bool ok;
    int l_flood = m_settings->value("Options/global_message_floodguard", 0).toInt(&ok);
    if (!ok) {
        qWarning("global_message_floodguard is not an int!");
        l_flood = 0;
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
    return m_discord->value("Discord/webhook_uptime_enabled", "false").toBool();
}

int ConfigManager::discordUptimeTime()
{
    bool ok;
    int l_aliveTime = m_discord->value("Discord/webhook_uptime_time", "60").toInt(&ok);
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

QString ConfigManager::discordWebhookColor()
{
    const QString l_default_color = "13312842";
    QString l_color = m_discord->value("Discord/webhook_color", l_default_color).toString();
    if (l_color.isEmpty()) {
        return l_default_color;
    }
    else {
        return l_color;
    }
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

QString ConfigManager::LogText(QString f_logtype)
{
    return m_logtext->value("LogConfiguration/" + f_logtype, "").toString();
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

QStringList ConfigManager::cdnList()
{
    return m_commands->cdns;
}

bool ConfigManager::advertiseServer()
{
    return m_settings->value("Advertiser/advertise", "true").toBool();
}

bool ConfigManager::advertiserDebug()
{
    return m_settings->value("Advertiser/debug", "true").toBool();
}

QUrl ConfigManager::advertiserIP()
{
    qDebug() << m_settings->value("Advertiser/ms_ip", "").toUrl();
    return m_settings->value("Advertiser/ms_ip", "").toUrl();
}

QString ConfigManager::advertiserHostname()
{
    return m_settings->value("Advertiser/hostname", "").toString();
}

bool ConfigManager::advertiserCloudflareMode()
{
    return m_settings->value("Advertiser/cloudflare_enabled", "false").toBool();
}

qint64 ConfigManager::uptime()
{
    return m_uptimeTimer->elapsed();
}

ConfigManager::help ConfigManager::commandHelp(QString f_command_name)
{
    return m_commands_help->value(f_command_name);
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
