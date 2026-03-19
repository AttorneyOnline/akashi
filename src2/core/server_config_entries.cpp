#include "core/server_config_entries.h"

using namespace akashi;

QList<ConfigEntry> serverConfigEntries()
{
    return {
        {"Options/bind_ip", "all", "The IP address to listen on, or all for every address."},
        {"Options/port", 27016, "The port to listen for incoming connections on.", inRange(1, 65535)},
        {"Options/secure_port", -1, "The port to advertise for SSL, or -1 for none.", inRange(-1, 65535)},
        {"Options/ms_port", 27016, "Unused legacy master server port.", inRange(1, 65535)},
        {"Options/webao_port", 27017, "The port advertised to WebAO users.", inRange(1, 65535)},
        {"Options/max_players", 100, "The maximum number of players that can join at once.", atLeast(1)},
        {"Options/server_name", "An Unnamed Server", "The server name shown on the master server."},
        {"Options/server_nickname", "", "The name used in messages sent by the server. Falls back to the server name."},
        {"Options/server_description", "This is a placeholder server description. Tell the world of AO who you are here!", "The server description shown on the master server."},
        {"Options/motd", "MOTD is not set.", "The message of the day sent to joining users."},
        {"Options/webao_enable", true, "Whether WebAO connections are accepted."},
        {"Options/auth", "simple", "The authorization type, simple or advanced.", oneOf({"simple", "advanced"})},
        {"Options/modpass", "changeme", "The moderator password used with simple authorization."},
        {"Options/logbuffer", 500, "The number of log messages an area stores.", atLeast(0)},
        {"Options/logging", "modcall", "The logging type: modcall, full or fullarea.", oneOf({"modcall", "full", "fullarea"})},
        {"Options/maximum_statements", 10, "The maximum number of statements the testimony recorder stores.", atLeast(0)},
        {"Options/multiclient_limit", 15, "The maximum number of connections from the same IP address.", atLeast(1)},
        {"Options/maximum_characters", 256, "The maximum length of chat messages.", atLeast(1)},
        {"Options/message_floodguard", 250, "The minimum time between messages in an area, in milliseconds.", atLeast(0)},
        {"Options/global_message_floodguard", 0, "The minimum time between messages in the whole server, in milliseconds.", atLeast(0)},
        {"Options/packet_rate_limit_soft", 10, "Sending packets faster than this warns the client."},
        {"Options/packet_rate_limit_hard", 20, "Sending packets faster than this disconnects the client."},
        {"Options/afk_timeout", 300, "Seconds without input before a player counts as AFK.", atLeast(1)},
        {"Options/asset_url", "", "The URL of the server's asset repository, used by WebAO users."},
        {"Advertiser/advertise", true, "Whether the server appears on the master server."},
        {"Advertiser/ms_ip", "https://servers.aceattorneyonline.com/servers", "The address of the master server."},
        {"Advertiser/hostname", "", "Optional hostname of the server, disables automatic IP detection."},
        {"Advertiser/cloudflare_enabled", false, "Whether the advertised WebAO port is rewritten to 80 for Cloudflare tunnels."},
        {"Dice/max_value", 100, "The maximum number of sides dice can have.", atLeast(1)},
        {"Dice/max_dice", 100, "The maximum number of dice rolled at once.", atLeast(1)},
        {"Password/password_requirements", true, "Whether password requirements are enforced under advanced authorization."},
        {"Password/pass_min_length", 8, "The minimum password length.", atLeast(0)},
        {"Password/pass_max_length", 0, "The maximum password length, or 0 for unlimited.", atLeast(0)},
        {"Password/pass_required_mix_case", true, "Whether passwords need both upper and lower case letters."},
        {"Password/pass_required_numbers", true, "Whether passwords need at least one number."},
        {"Password/pass_required_special", true, "Whether passwords need at least one special character."},
        {"Password/pass_can_contain_username", false, "Whether passwords may contain the username."},
    };
}

QList<ConfigEntry> discordConfigEntries()
{
    return {
        {"Discord/webhook_enabled", false, "Whether Discord webhooks are enabled at all."},
        {"Discord/webhook_modcall_enabled", false, "Whether modcalls are sent to a webhook."},
        {"Discord/webhook_modcall_url", "", "The webhook URL for modcalls."},
        {"Discord/webhook_modcall_content", "", "Extra text sent with a modcall, for example a role ping."},
        {"Discord/webhook_modcall_sendfile", false, "Whether the area log is attached to a modcall."},
        {"Discord/webhook_ban_enabled", false, "Whether bans are sent to a webhook."},
        {"Discord/webhook_ban_url", "", "The webhook URL for bans."},
        {"Discord/webhook_color", "13312842", "The color of webhook messages."},
    };
}
