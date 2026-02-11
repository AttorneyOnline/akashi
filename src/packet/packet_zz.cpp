#include "packet/packet_zz.h"
#include "config_manager.h"
#include "discordhook.h"
#include "packet/packet_factory.h"
#include "server.h"
#include "serviceregistry.h"

#include <QQueue>

PacketZZ::PacketZZ(QStringList &contents) :
    AOPacket(contents)
{
}

PacketInfo PacketZZ::getPacketInfo() const
{
    PacketInfo info{
        .acl_permission = ACLRole::Permission::NONE,
        .min_args = 2,
        .header = "ZZ"};
    return info;
}

void PacketZZ::handlePacket(AreaData *area, AOClient &client) const
{
    QString l_name = client.name();
    if (client.name().isEmpty())
        l_name = client.character();

    QString l_areaName = area->name();

    QString l_modcallNotice = "!!!MODCALL!!!\nArea: " + l_areaName + "\nCaller: " + l_name + "\n";

    int target_id = m_content.at(1).toInt();
    if (target_id != -1) {
        AOClient *target = client.getServer()->getClientByID(target_id);
        if (target) {
            l_modcallNotice.append("Regarding: " + target->name() + "\n");
        }
    }
    l_modcallNotice.append("Reason: " + m_content[0]);

    const QVector<AOClient *> l_clients = client.getServer()->getClients();
    for (AOClient *l_client : l_clients) {
        if (l_client->m_authenticated)
            l_client->sendPacket(PacketFactory::createPacket("ZZ", {l_modcallNotice}));
    }
    emit client.logModcall((client.character() + " " + client.characterName()), client.m_ipid, client.name(), client.getServer()->getAreaById(client.areaId())->name());

    if (ConfigManager::discordModcallWebhookEnabled() && client.m_service_registry->exists(DiscordHook::SERVICE_ID)) {
        QString l_name = client.name();
        if (client.name().isEmpty())
            l_name = client.character();

        QString l_areaName = area->name();

        QString webhook_reason = m_content.value(0);
        if (target_id != -1) {
            AOClient *target = client.getServer()->getClientByID(target_id);
            if (target) {
                webhook_reason.append(" (Regarding: " + target->name() + ")");
            }
        }

        DiscordMessage l_message;
        l_message.setContent(ConfigManager::discordModcallWebhookContent())
            .beginEmbed()
            .setEmbedColor(ConfigManager::discordWebhookColor())
            .setEmbedTitle(l_name + " filed a modcall in " + l_areaName)
            .setEmbedDescription(webhook_reason)
            .endEmbed();

        const auto l_data = client.getServer()->getAreaBuffer(l_areaName);
        QString l_log;

        for (const QString &l_entry : l_data) {
            l_log.append(l_entry);
        }
        DiscordMultipartMessage l_multi_message;
        l_multi_message.setRequestUrl(ConfigManager::discordModcallWebhookUrl());
        l_multi_message.addPart(l_log.toUtf8(), "file", "log.txt", "text/plain", "utf-8").setPayloadJson(l_message.toJson());

        client.m_service_registry->get<DiscordHook>(DiscordHook::SERVICE_ID).value()->post(l_multi_message);
    }
}
