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

#include "akashi/config_store.h"
#include "core/server_settings.h"

#include <QSqlDatabase>
#include <QSqlQuery>

akashi::ConfigStore *ConfigManager::m_store = nullptr;
ServerSettings *ConfigManager::m_server_settings = nullptr;
DiscordSettings *ConfigManager::m_discord_settings = nullptr;
QSettings *ConfigManager::m_areas = nullptr;
QSettings *ConfigManager::m_logtext = nullptr;
QSettings *ConfigManager::m_ambience = nullptr;
ConfigManager::CommandSettings *ConfigManager::m_commands = new CommandSettings();
MusicList *ConfigManager::m_musicList = new MusicList;
QHash<QString, ConfigManager::help> *ConfigManager::m_commands_help = new QHash<QString, ConfigManager::help>;
QStringList *ConfigManager::m_ordered_list = new QStringList;

bool ConfigManager::setStore(akashi::ConfigStore *f_store)
{
    m_store = f_store;
    m_server_settings = new ServerSettings(f_store);
    m_discord_settings = new DiscordSettings(f_store);
    const bool l_valid = m_server_settings->declare() && m_discord_settings->declare();

    m_areas = f_store->settings("areas");
    m_logtext = f_store->settings("text/logtext");
    m_ambience = f_store->settings("ambience");

    // Opened here so leftover INI files are converted before their loaders run.
    f_store->settings("acl_roles");
    f_store->settings("command_extensions");

    return l_valid;
}

QString ConfigManager::path(const QString &f_file_name)
{
    return m_store ? m_store->filePath(f_file_name) : "config/" + f_file_name;
}

bool ConfigManager::verifyServerConfig()
{
    // Verify directories
    QStringList l_directories{path(""), path("text/")};
    for (const QString &l_directory : l_directories) {
        if (!dirExists(QFileInfo(l_directory))) {
            qCritical() << l_directory + " does not exist!";
            return false;
        }
    }

    // Verify config files
    QStringList l_config_files{path("config.json"), path("areas.json"), path("backgrounds.txt"), path("characters.txt"), path("music.json"),
                               path("discord.json"), path("text/8ball.txt"), path("text/gimp.txt"), path("text/praise.txt"),
                               path("text/reprimands.txt"), path("text/commandhelp.json"), path("text/cdns.txt"), path("ipbans.json")};
    for (const QString &l_file : l_config_files) {
        if (!fileExists(QFileInfo(l_file))) {
            qCritical() << l_file + " does not exist!";
            return false;
        }
    }

    // Verify areas
    if (m_areas->childGroups().length() < 1) {
        qCritical() << "areas.json is invalid!";
        return false;
    }

    // Read dices
    QSettings &l_dice_ini = *m_store->settings("dice");
    QStringList dices = l_dice_ini.childGroups();

    for (const QString &dice : dices) {
        l_dice_ini.beginGroup(dice);

        int max = l_dice_ini.value("max").toInt();
        QStringList faces;

        for (int i = 1; i <= max; ++i) {
            QString key = QString::number(i);
            if (l_dice_ini.contains(key)) {
                faces.append(l_dice_ini.value(key).toString());
            }
            else {
                qCritical() << "dice.ini max mismatch!";
                break;
            }
        }
        m_commands->dice_faces[dice] = faces;
        l_dice_ini.endGroup();
    }

    // Every single setting was already checked by the store, only the relation between the limits remains.
    const int l_soft_limit = m_server_settings->packet_rate_limit_soft();
    const int l_hard_limit = m_server_settings->packet_rate_limit_hard();
    if (l_soft_limit > 0 && l_hard_limit <= l_soft_limit) {
        qCritical("packet_rate_limit_hard must be greater than packet_rate_limit_soft!");
        return false;
    }
    if (l_soft_limit <= 0) {
        qWarning("packet_rate_limit_soft is 0 or less, warning threshold is disabled!");
    }
    if (l_hard_limit <= 0) {
        qWarning("packet_rate_limit_hard is 0 or less, rate limiting is disabled!");
    }
    m_commands->magic_8ball = (loadConfigFile("8ball"));
    m_commands->praises = (loadConfigFile("praise"));
    m_commands->reprimands = (loadConfigFile("reprimands"));
    m_commands->gimps = (loadConfigFile("gimp"));
    m_commands->filters = (loadConfigFile("filter"));
    m_commands->cdns = (loadConfigFile("cdns"));
    if (m_commands->cdns.isEmpty())
        m_commands->cdns = QStringList{"cdn.discord.com"};

    return true;
}

QString ConfigManager::bindIP()
{
    return m_server_settings->bind_ip();
}

QStringList ConfigManager::charlist()
{
    QStringList l_charlist;
    QFile l_file(path("characters.txt"));
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
    QFile l_file(path("backgrounds.txt"));
    l_file.open(QIODevice::ReadOnly | QIODevice::Text);
    while (!l_file.atEnd()) {
        l_backgrounds.append(l_file.readLine().trimmed());
    }
    l_file.close();

    return l_backgrounds;
}

MusicList ConfigManager::musiclist()
{
    QFile l_music_json(path("music.json"));
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
    QFile l_help_json(path("text/commandhelp.json"));
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
    QFile l_json_file(path("ipbans.json"));
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
        // The connection is reused on reload instead of being added again.
        QSqlDatabase asn_db = QSqlDatabase::contains("ASN") ? QSqlDatabase::database("ASN") : QSqlDatabase::addDatabase("QSQLITE", "ASN");
        asn_db.setDatabaseName("storage/asn.sqlite3");
        asn_db.open();

        const QStringList l_asns = l_json_obj["asn"].toVariant().toStringList();
        QStringList l_placeholders;
        for (int i = 0; i < l_asns.size(); i++) {
            l_placeholders.append("?");
        }
        QSqlQuery query(asn_db);
        query.prepare("SELECT ip FROM maxmind WHERE asn in (" + l_placeholders.join(",") + ")");
        for (const QString &l_asn : l_asns) {
            query.addBindValue(l_asn);
        }
        query.exec();
        while (query.next()) {
            l_range_bans.append(query.value(0).toString());
        }
        asn_db.close();
    }
    l_range_bans.removeDuplicates();
    return l_range_bans;
}

QList<quint32> ConfigManager::bannedAsns()
{
    QFile l_file(path("ipbans.json"));
    l_file.open(QIODevice::ReadOnly | QIODevice::Text);
    const QJsonObject l_root = QJsonDocument::fromJson(l_file.readAll()).object();

    QList<quint32> l_asns;
    const QStringList l_texts = l_root["asn"].toVariant().toStringList();
    for (const QString &l_text : l_texts) {
        l_asns.append(l_text.toUInt());
    }
    return l_asns;
}

void ConfigManager::reloadSettings()
{
    // Cleared so removed songs do not survive the reload.
    m_musicList->clear();
    m_ordered_list->clear();
    m_store->reload();
}

QStringList ConfigManager::loadConfigFile(const QString filename)
{
    QStringList stringlist;
    QFile l_file(path("text/" + filename + ".txt"));
    l_file.open(QIODevice::ReadOnly | QIODevice::Text);
    while (!(l_file.atEnd())) {
        stringlist.append(l_file.readLine().trimmed());
    }
    l_file.close();
    return stringlist;
}

int ConfigManager::maxPlayers()
{
    int l_players = m_server_settings->max_players();
    return l_players;
}

int ConfigManager::serverPort()
{
    if (m_store->settings("config")->contains("Options/webao_port")) {
        qWarning("webao_port is deprecated, use port instead");
        return m_server_settings->webao_port();
    }

    return m_server_settings->port();
}

int ConfigManager::securePort()
{
    return m_server_settings->secure_port();
}

QString ConfigManager::serverDescription()
{
    return m_server_settings->server_description();
}

QString ConfigManager::serverName()
{
    return m_server_settings->server_name();
}

QString ConfigManager::serverNickname()
{
    QString l_tag = m_server_settings->server_nickname();
    return l_tag.isEmpty() ? serverName() : l_tag;
}

QString ConfigManager::motd()
{
    return m_server_settings->motd();
}

QTime ConfigManager::maintenanceTime()
{
    return m_server_settings->maintenance_time();
}

bool ConfigManager::maintenanceVacuum()
{
    return m_server_settings->maintenance_vacuum();
}

int ConfigManager::maintenanceMaxPlayers()
{
    return m_server_settings->maintenance_max_players();
}

bool ConfigManager::webaoEnabled()
{
    return m_server_settings->webao_enable();
}

DataTypes::AuthType ConfigManager::authType()
{
    QString l_auth = m_server_settings->auth().toUpper();
    return toDataType<DataTypes::AuthType>(l_auth);
}

QString ConfigManager::modpass()
{
    return m_server_settings->modpass();
}

int ConfigManager::logBuffer()
{
    int l_buffer = m_server_settings->logbuffer();
    return l_buffer;
}

DataTypes::LogType ConfigManager::loggingType()
{
    QString l_log = m_server_settings->logging().toUpper();
    return toDataType<DataTypes::LogType>(l_log);
}

int ConfigManager::maxStatements()
{
    int l_max = m_server_settings->maximum_statements();
    return l_max;
}
int ConfigManager::multiClientLimit()
{
    int l_limit = m_server_settings->multiclient_limit();
    return l_limit;
}

int ConfigManager::maxCharacters()
{
    int l_max = m_server_settings->maximum_characters();
    return l_max;
}

int ConfigManager::messageFloodguard()
{
    int l_flood = m_server_settings->message_floodguard();
    return l_flood;
}

int ConfigManager::globalMessageFloodguard()
{
    int l_flood = m_server_settings->global_message_floodguard();
    return l_flood;
}

int ConfigManager::packetRateLimitSoft()
{
    int l_limit = m_server_settings->packet_rate_limit_soft();
    return l_limit;
}

int ConfigManager::packetRateLimitHard()
{
    int l_limit = m_server_settings->packet_rate_limit_hard();
    return l_limit;
}

QUrl ConfigManager::assetUrl()
{
    QByteArray l_url = m_server_settings->asset_url().toUtf8();
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
    int l_value = m_server_settings->max_value();
    return l_value;
}

int ConfigManager::diceMaxDice()
{
    int l_dice = m_server_settings->max_dice();
    return l_dice;
}

bool ConfigManager::discordWebhookEnabled()
{
    return m_discord_settings->webhook_enabled();
}

bool ConfigManager::discordModcallWebhookEnabled()
{
    return m_discord_settings->webhook_modcall_enabled();
}

QString ConfigManager::discordModcallWebhookUrl()
{
    return m_discord_settings->webhook_modcall_url();
}

QString ConfigManager::discordModcallWebhookContent()
{
    return m_discord_settings->webhook_modcall_content();
}

bool ConfigManager::discordModcallWebhookSendFile()
{
    return m_discord_settings->webhook_modcall_sendfile();
}

bool ConfigManager::discordBanWebhookEnabled()
{
    return m_discord_settings->webhook_ban_enabled();
}

QString ConfigManager::discordBanWebhookUrl()
{
    return m_discord_settings->webhook_ban_url();
}

QString ConfigManager::discordWebhookColor()
{
    const QString l_default_color = "13312842";
    QString l_color = m_discord_settings->webhook_color();
    if (l_color.isEmpty()) {
        return l_default_color;
    }
    else {
        return l_color;
    }
}

bool ConfigManager::passwordRequirements()
{
    return m_server_settings->password_requirements();
}

int ConfigManager::passwordMinLength()
{
    int l_min = m_server_settings->pass_min_length();
    return l_min;
}

int ConfigManager::passwordMaxLength()
{
    int l_max = m_server_settings->pass_max_length();
    return l_max;
}

bool ConfigManager::passwordRequireMixCase()
{
    return m_server_settings->pass_required_mix_case();
}

bool ConfigManager::passwordRequireNumbers()
{
    return m_server_settings->pass_required_numbers();
}

bool ConfigManager::passwordRequireSpecialCharacters()
{
    return m_server_settings->pass_required_special();
}

bool ConfigManager::passwordCanContainUsername()
{
    return m_server_settings->pass_can_contain_username();
}

QString ConfigManager::LogText(QString f_logtype)
{
    return m_logtext->value("LogConfiguration/" + f_logtype, "").toString();
}

int ConfigManager::afkTimeout()
{
    int l_afk = m_server_settings->afk_timeout();
    return l_afk;
}

void ConfigManager::setAuthType(const DataTypes::AuthType f_auth)
{
    m_server_settings->auth.set(fromDataType<DataTypes::AuthType>(f_auth).toLower());
}

QStringList ConfigManager::diceFaces(const QString f_name)
{
    return m_commands->dice_faces[f_name];
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

QStringList ConfigManager::filterList()
{
    return m_commands->filters;
}

QStringList ConfigManager::cdnList()
{
    return m_commands->cdns;
}

bool ConfigManager::publishServerEnabled()
{
    return m_server_settings->advertise();
}

QUrl ConfigManager::serverlistURL()
{
    return QUrl(m_server_settings->ms_ip());
}

QString ConfigManager::serverDomainName()
{
    return m_server_settings->hostname();
}

bool ConfigManager::advertiseWSProxy()
{
    return m_server_settings->cloudflare_enabled();
}

ConfigManager::help ConfigManager::commandHelp(QString f_command_name)
{
    return m_commands_help->value(f_command_name);
}

void ConfigManager::setMotd(const QString f_motd)
{
    m_server_settings->motd.set(f_motd);
}

bool ConfigManager::fileExists(const QFileInfo &f_file)
{
    return (f_file.exists() && f_file.isFile());
}

bool ConfigManager::dirExists(const QFileInfo &f_dir)
{
    return (f_dir.exists() && f_dir.isDir());
}
