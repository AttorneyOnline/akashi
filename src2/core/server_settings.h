#ifndef SERVER_SETTINGS_H
#define SERVER_SETTINGS_H

#include "akashi/settings.h"

#include <QTime>

// The settings of config.json. Every setting is declared exactly once, here.
class AKASHI_CORE_EXPORT ServerSettings : public akashi::Settings
{
  public:
    explicit ServerSettings(akashi::ConfigStore *f_store) :
        Settings(f_store, "config") {}

    akashi::Setting<QString> bind_ip{this, "Options/bind_ip", "all", "The IP address to listen on, or all for every address."};
    akashi::Setting<int> port{this, "Options/port", 27016, "The port to listen for incoming connections on.", akashi::inRange(1, 65535)};
    akashi::Setting<int> secure_port{this, "Options/secure_port", -1, "The port to advertise for SSL, or -1 for none.", akashi::inRange(-1, 65535)};
    akashi::Setting<int> ms_port{this, "Options/ms_port", 27016, "Unused legacy master server port.", akashi::inRange(1, 65535)};
    akashi::Setting<int> webao_port{this, "Options/webao_port", 27017, "The port advertised to WebAO users.", akashi::inRange(1, 65535)};
    akashi::Setting<int> max_players{this, "Options/max_players", 100, "The maximum number of players that can join at once.", akashi::atLeast(1)};
    akashi::Setting<QString> server_name{this, "Options/server_name", "An Unnamed Server", "The server name shown on the master server."};
    akashi::Setting<QString> server_nickname{this, "Options/server_nickname", "", "The name used in messages sent by the server. Falls back to the server name."};
    akashi::Setting<QString> server_description{this, "Options/server_description", "This is a placeholder server description. Tell the world of AO who you are here!", "The server description shown on the master server."};
    akashi::Setting<QString> motd{this, "Options/motd", "MOTD is not set.", "The message of the day sent to joining users."};
    akashi::Setting<bool> webao_enable{this, "Options/webao_enable", true, "Whether WebAO connections are accepted."};
    akashi::Setting<QString> auth{this, "Options/auth", "simple", "The authorization type, simple or advanced.", akashi::oneOf({"simple", "advanced"})};
    akashi::Setting<QString> modpass{this, "Options/modpass", "changeme", "The moderator password used with simple authorization."};
    akashi::Setting<int> logbuffer{this, "Options/logbuffer", 500, "The number of log messages an area stores.", akashi::atLeast(0)};
    akashi::Setting<QString> logging{this, "Options/logging", "modcall", "The logging type: modcall, full or fullarea.", akashi::oneOf({"modcall", "full", "fullarea"})};
    akashi::Setting<int> maximum_statements{this, "Options/maximum_statements", 10, "The maximum number of statements the testimony recorder stores.", akashi::atLeast(0)};
    akashi::Setting<int> multiclient_limit{this, "Options/multiclient_limit", 15, "The maximum number of connections from the same IP address.", akashi::atLeast(1)};
    akashi::Setting<int> reconnect_grace{this, "Options/reconnect_grace", 0, "How many seconds a client that lost its connection keeps its place for a reconnect. 0 removes it immediately; while a client waits, its character stays taken.", akashi::atLeast(0)};
    akashi::Setting<QString> id_assignment{this, "Options/id_assignment", "last_freed", "How player IDs are handed out: last_freed gives a new arrival the most recently freed ID, lowest always the lowest free one.", akashi::oneOf({"last_freed", "lowest"})};
    akashi::Setting<int> maximum_characters{this, "Options/maximum_characters", 256, "The maximum length of chat messages.", akashi::atLeast(1)};
    akashi::Setting<int> message_floodguard{this, "Options/message_floodguard", 250, "The minimum time between messages in an area, in milliseconds.", akashi::atLeast(0)};
    akashi::Setting<int> global_message_floodguard{this, "Options/global_message_floodguard", 0, "The minimum time between messages in the whole server, in milliseconds.", akashi::atLeast(0)};
    akashi::Setting<int> packet_rate_limit_soft{this, "Options/packet_rate_limit_soft", 10, "Sending packets faster than this warns the client."};
    akashi::Setting<int> packet_rate_limit_hard{this, "Options/packet_rate_limit_hard", 20, "Sending packets faster than this disconnects the client."};
    akashi::Setting<int> afk_timeout{this, "Options/afk_timeout", 300, "Seconds without input before a player counts as AFK.", akashi::atLeast(1)};
    akashi::Setting<QString> asset_url{this, "Options/asset_url", "", "The URL of the server's asset repository, used by WebAO users."};
    akashi::Setting<bool> advertise{this, "Advertiser/advertise", true, "Whether the server appears on the master server."};
    akashi::Setting<QString> ms_ip{this, "Advertiser/ms_ip", "https://servers.aceattorneyonline.com/servers", "The address of the master server."};
    akashi::Setting<QString> hostname{this, "Advertiser/hostname", "", "Optional hostname of the server, disables automatic IP detection."};
    akashi::Setting<bool> cloudflare_enabled{this, "Advertiser/cloudflare_enabled", false, "Whether the advertised WebAO port is rewritten to 80 for Cloudflare tunnels."};
    akashi::Setting<int> max_value{this, "Dice/max_value", 100, "The maximum number of sides dice can have.", akashi::atLeast(1)};
    akashi::Setting<int> max_dice{this, "Dice/max_dice", 100, "The maximum number of dice rolled at once.", akashi::atLeast(1)};
    akashi::Setting<bool> password_requirements{this, "Password/password_requirements", true, "Whether password requirements are enforced under advanced authorization."};
    akashi::Setting<int> pass_min_length{this, "Password/pass_min_length", 8, "The minimum password length.", akashi::atLeast(0)};
    akashi::Setting<int> pass_max_length{this, "Password/pass_max_length", 0, "The maximum password length, or 0 for unlimited.", akashi::atLeast(0)};
    akashi::Setting<bool> pass_required_mix_case{this, "Password/pass_required_mix_case", true, "Whether passwords need both upper and lower case letters."};
    akashi::Setting<bool> pass_required_numbers{this, "Password/pass_required_numbers", true, "Whether passwords need at least one number."};
    akashi::Setting<bool> pass_required_special{this, "Password/pass_required_special", true, "Whether passwords need at least one special character."};
    akashi::Setting<bool> pass_can_contain_username{this, "Password/pass_can_contain_username", false, "Whether passwords may contain the username."};
    akashi::Setting<QTime> maintenance_time{this, "Database/maintenance_time", QTime(), "The daily time when database maintenance runs, for example 04:30. Empty to disable.", akashi::emptyOrTime()};
    akashi::Setting<bool> maintenance_vacuum{this, "Database/maintenance_vacuum", false, "Whether maintenance also compacts the databases with VACUUM."};
    akashi::Setting<int> maintenance_max_players{this, "Database/maintenance_max_players", -1, "Maintenance waits while more players than this are online, or -1 to run regardless.", akashi::atLeast(-1)};
};

// The settings of discord.json.
class AKASHI_CORE_EXPORT DiscordSettings : public akashi::Settings
{
  public:
    explicit DiscordSettings(akashi::ConfigStore *f_store) :
        Settings(f_store, "discord") {}

    akashi::Setting<bool> webhook_enabled{this, "Discord/webhook_enabled", false, "Whether Discord webhooks are enabled at all."};
    akashi::Setting<bool> webhook_modcall_enabled{this, "Discord/webhook_modcall_enabled", false, "Whether modcalls are sent to a webhook."};
    akashi::Setting<QString> webhook_modcall_url{this, "Discord/webhook_modcall_url", "", "The webhook URL for modcalls."};
    akashi::Setting<QString> webhook_modcall_content{this, "Discord/webhook_modcall_content", "", "Extra text sent with a modcall, for example a role ping."};
    akashi::Setting<bool> webhook_modcall_sendfile{this, "Discord/webhook_modcall_sendfile", false, "Whether the area log is attached to a modcall."};
    akashi::Setting<bool> webhook_ban_enabled{this, "Discord/webhook_ban_enabled", false, "Whether bans are sent to a webhook."};
    akashi::Setting<QString> webhook_ban_url{this, "Discord/webhook_ban_url", "", "The webhook URL for bans."};
    akashi::Setting<QString> webhook_color{this, "Discord/webhook_color", "13312842", "The color of webhook messages."};
};

#endif // SERVER_SETTINGS_H
