#include "discord_plugin.h"

#include "akashi/config_store.h"
#include "akashi/event.h"
#include "akashi/logging_categories.h"
#include "akashi/service_registry.h"
#include "core/discord_hook.h"
#include "core/log_service.h"
#include "world/rule_registry.h"

#include <QDebug>

QString DiscordPlugin::id() const { return QStringLiteral("akashi.discord"); }
akashi::ServiceVersion DiscordPlugin::pluginVersion() const { return {1, 0, 0}; }

bool DiscordPlugin::load(akashi::ServiceRegistry &services)
{
    auto l_config = services.resolve<akashi::ConfigStore>(QStringLiteral("akashi.config"));
    auto l_rules = services.resolve<akashi::RuleRegistry>(QStringLiteral("akashi.rules"));
    auto l_hook = services.resolve<akashi::DiscordHook>(QStringLiteral("akashi.discordhook"));
    auto l_log = services.resolve<akashi::LogService>(QStringLiteral("akashi.log"));

    if (!l_config || !l_rules || !l_hook) {
        qCWarning(akashiDiscord) << "discord: required services not available";
        return false;
    }

    const QString l_cfg = QStringLiteral("plugins/") + id();
    l_config->declarePlugin(id(), {
                                      akashi::ConfigEntry(QStringLiteral("webhook_enabled"), false,
                                                          QStringLiteral("Whether Discord webhooks are enabled at all.")),
                                      akashi::ConfigEntry(QStringLiteral("webhook_modcall_enabled"), false,
                                                          QStringLiteral("Whether modcalls are sent to a webhook.")),
                                      akashi::ConfigEntry(QStringLiteral("webhook_modcall_url"), QString(),
                                                          QStringLiteral("The webhook URL for modcalls."), akashi::emptyOr(akashi::url())),
                                      akashi::ConfigEntry(QStringLiteral("webhook_modcall_content"), QString(),
                                                          QStringLiteral("Extra text sent with a modcall, for example a role ping.")),
                                      akashi::ConfigEntry(QStringLiteral("webhook_modcall_sendfile"), false,
                                                          QStringLiteral("Whether the area log is attached to a modcall.")),
                                      akashi::ConfigEntry(QStringLiteral("webhook_ban_enabled"), false,
                                                          QStringLiteral("Whether bans are sent to a webhook.")),
                                      akashi::ConfigEntry(QStringLiteral("webhook_ban_url"), QString(),
                                                          QStringLiteral("The webhook URL for bans."), akashi::emptyOr(akashi::url())),
                                      akashi::ConfigEntry(QStringLiteral("webhook_color"), QStringLiteral("13312842"),
                                                          QStringLiteral("The color of webhook messages, as a decimal color code."), akashi::emptyOr(akashi::inRange(0, 16777215))),
                                  });

    // The payload keys are the event struct's fields; eventToMap put them
    // there under the same names the typed subscription used to see.
    l_rules->registerObserver(
        akashi::ModcallEvent::id, 0,
        [l_config, l_cfg, l_hook, l_log](const akashi::RuleContext &f_ctx) {
            if (!l_config->get<bool>(l_cfg, QStringLiteral("webhook_enabled")) ||
                !l_config->get<bool>(l_cfg, QStringLiteral("webhook_modcall_enabled")))
                return;

            const QVariantMap &e = f_ctx.payload;
            const QString l_ooc_name = e.value(QStringLiteral("ooc_name")).toString();
            const QString l_area_name = e.value(QStringLiteral("area_name")).toString();

            akashi::DiscordMessage l_message;
            const QString l_content = l_config->get<QString>(l_cfg, QStringLiteral("webhook_modcall_content"));
            if (!l_content.isEmpty())
                l_message.setContent(l_content);
            l_message.setRequestUrl(l_config->get<QString>(l_cfg, QStringLiteral("webhook_modcall_url")))
                .beginEmbed()
                .setEmbedColor(l_config->get<QString>(l_cfg, QStringLiteral("webhook_color")))
                .setEmbedTitle(QStringLiteral("[%1]%2 filed a modcall in %3")
                                   .arg(e.value(QStringLiteral("client_id")).toString(),
                                        l_ooc_name.isEmpty() ? e.value(QStringLiteral("char_name")).toString() : l_ooc_name,
                                        l_area_name))
                .setEmbedDescription(e.value(QStringLiteral("reason")).toString())
                .endEmbed();

            // With sendfile on, the area log rides along in one multipart
            // post; without it, the message goes out on its own.
            if (l_config->get<bool>(l_cfg, QStringLiteral("webhook_modcall_sendfile")) && l_log) {
                QString l_text;
                const auto l_recent = l_log->recentEvents(l_area_name, 0);
                for (const auto &l_event : l_recent)
                    l_text.append(l_log->formatEvent(l_event) + QStringLiteral("\n"));

                if (!l_text.isEmpty()) {
                    akashi::DiscordMultipartMessage l_multipart;
                    l_multipart.setRequestUrl(l_message.requestUrl())
                        .addPart(l_text.toUtf8(), QStringLiteral("file"), QStringLiteral("log.txt"),
                                 QStringLiteral("text/plain"), QStringLiteral("utf-8"))
                        .setPayloadJson(l_message.toJson());
                    l_hook->post(l_multipart);
                    return;
                }
            }
            l_hook->post(l_message);
        },
        id());

    l_rules->registerObserver(
        akashi::BanIssuedEvent::id, 0,
        [l_config, l_cfg, l_hook](const akashi::RuleContext &f_ctx) {
            if (!l_config->get<bool>(l_cfg, QStringLiteral("webhook_enabled")) ||
                !l_config->get<bool>(l_cfg, QStringLiteral("webhook_ban_enabled")))
                return;

            const QVariantMap &e = f_ctx.payload;
            akashi::DiscordMessage l_message;
            l_message.setRequestUrl(l_config->get<QString>(l_cfg, QStringLiteral("webhook_ban_url")))
                .beginEmbed()
                .setEmbedColor(l_config->get<QString>(l_cfg, QStringLiteral("webhook_color")))
                .setEmbedTitle(QStringLiteral("Ban issued by ") + e.value(QStringLiteral("moderator")).toString())
                .addEmbedField(QStringLiteral("IPID"), e.value(QStringLiteral("target_ipid")).toString(), true)
                .addEmbedField(QStringLiteral("Ban ID"), e.value(QStringLiteral("ban_id")).toString(), true)
                .addEmbedField(QStringLiteral("Banned until"), e.value(QStringLiteral("duration")).toString(), true)
                .addEmbedField(QStringLiteral("Reason"), e.value(QStringLiteral("reason")).toString())
                .endEmbed();
            l_hook->post(l_message);
        },
        id());

    return true;
}

void DiscordPlugin::shutdown(akashi::ServiceRegistry &services)
{
    auto l_rules = services.resolve<akashi::RuleRegistry>(QStringLiteral("akashi.rules"));
    if (l_rules)
        l_rules->unregisterObservers(id());
}
