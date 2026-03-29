#pragma once

#include "config_manager.h"
#include "discordtypes.h"

#include <QQueue>

/**
 * @brief DiscordMessageHelper contains an assortment of helper function to create specific Webhook messages.
 */
namespace DiscordMessageHelper {

inline const DiscordMessage banMessage(const QString &f_ipid, const QString &f_moderator, const QString &f_duration, const QString &f_reason, const int &f_banID)
{
    DiscordMessage l_message;

    return l_message;
}

inline const DiscordMultipartMessage modcallMessage(const QString &f_name, const QString &f_area, const QString &f_reason, const QQueue<QString> &f_buffer)
{
    DiscordMultipartMessage l_multi_message;

    DiscordMessage l_message;
    l_message.setContent(ConfigManager::discordModcallWebhookContent())
        .beginEmbed()
        .setEmbedColor(ConfigManager::discordWebhookColor())
        .setEmbedTitle(f_name + " filed a modcall in " + f_area)
        .setEmbedDescription(f_reason)
        .endEmbed();

    QString l_log;
    for (const QString &l_entry : f_buffer) {
        l_log.append(l_entry);
    }
    l_multi_message.setRequestUrl(ConfigManager::discordModcallWebhookUrl());
    l_multi_message.addPart(l_log.toUtf8(), "file", "log.txt", "text/plain", "utf-8").setPayloadJson(l_message.toJson());

    return l_multi_message;
}

}
