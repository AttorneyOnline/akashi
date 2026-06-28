#include "discord_plugin.h"

#include "webhook_sender.h"

#include "akashi/config_store.h"
#include "akashi/event.h"
#include "akashi/network_service.h"
#include "akashi/service_registry.h"
#include "core/event_bus.h"
#include "core/log_service.h"

#include <QDebug>

QString DiscordPlugin::id() const { return QStringLiteral("akashi.discord"); }
akashi::ServiceVersion DiscordPlugin::pluginVersion() const { return {1, 0, 0}; }

bool DiscordPlugin::load(akashi::ServiceRegistry &services)
{
    auto l_config = services.resolve<akashi::ConfigStore>(QStringLiteral("akashi.config"));
    auto l_events = services.resolve<akashi::EventBus>(QStringLiteral("akashi.events"));
    auto l_network = services.resolve<akashi::NetworkService>(QStringLiteral("akashi.network"));
    auto l_log = services.resolve<akashi::LogService>(QStringLiteral("akashi.log"));

    if (!l_config || !l_events || !l_network) {
        qWarning() << "discord: required services not available";
        return false;
    }

    const QString l_cfg = QStringLiteral("plugins/") + id();
    l_config->declarePlugin(id(), {
        akashi::ConfigEntry(QStringLiteral("webhook_enabled"), false,
                            QStringLiteral("Whether Discord webhooks are enabled at all.")),
        akashi::ConfigEntry(QStringLiteral("webhook_modcall_enabled"), false,
                            QStringLiteral("Whether modcalls are sent to a webhook.")),
        akashi::ConfigEntry(QStringLiteral("webhook_modcall_url"), QString(),
                            QStringLiteral("The webhook URL for modcalls.")),
        akashi::ConfigEntry(QStringLiteral("webhook_modcall_content"), QString(),
                            QStringLiteral("Extra text sent with a modcall, for example a role ping.")),
        akashi::ConfigEntry(QStringLiteral("webhook_modcall_sendfile"), false,
                            QStringLiteral("Whether the area log is attached to a modcall.")),
        akashi::ConfigEntry(QStringLiteral("webhook_ban_enabled"), false,
                            QStringLiteral("Whether bans are sent to a webhook.")),
        akashi::ConfigEntry(QStringLiteral("webhook_ban_url"), QString(),
                            QStringLiteral("The webhook URL for bans.")),
        akashi::ConfigEntry(QStringLiteral("webhook_color"), QStringLiteral("13312842"),
                            QStringLiteral("The color of webhook messages.")),
    });

    m_sender = std::make_unique<WebhookSender>(l_network->networkManager(), this);

    auto applyConfig = [this, l_config, l_cfg]() {
        m_sender->setModcallUrl(l_config->get<QString>(l_cfg, QStringLiteral("webhook_modcall_url")));
        m_sender->setModcallContent(l_config->get<QString>(l_cfg, QStringLiteral("webhook_modcall_content")));
        m_sender->setModcallSendfile(l_config->get<bool>(l_cfg, QStringLiteral("webhook_modcall_sendfile")));
        m_sender->setBanUrl(l_config->get<QString>(l_cfg, QStringLiteral("webhook_ban_url")));
        m_sender->setColor(l_config->get<QString>(l_cfg, QStringLiteral("webhook_color")));
    };
    applyConfig();

    if (l_log) {
        m_sender->setAreaLogFn([l_log](const QString &f_area) -> QString {
            const auto l_events = l_log->recentEvents(f_area, 0);
            QString l_text;
            for (const auto &e : l_events)
                l_text.append(l_log->formatEvent(e) + QStringLiteral("\n"));
            return l_text;
        });
    }

    l_events->subscribe<akashi::ModcallEvent>(
        akashi::EventPhase::After, 0,
        [this, l_config, l_cfg](akashi::ModcallEvent &e) {
            if (!l_config->get<bool>(l_cfg, QStringLiteral("webhook_enabled")) ||
                !l_config->get<bool>(l_cfg, QStringLiteral("webhook_modcall_enabled")))
                return;

            ModcallPayload l_payload;
            l_payload.client_id = e.client_id;
            l_payload.name = e.ooc_name.isEmpty() ? e.char_name : e.ooc_name;
            l_payload.area_name = e.area_name;
            l_payload.reason = e.reason;
            m_sender->sendModcall(l_payload);
        },
        id());

    l_events->subscribe<akashi::BanIssuedEvent>(
        akashi::EventPhase::After, 0,
        [this, l_config, l_cfg](akashi::BanIssuedEvent &e) {
            if (!l_config->get<bool>(l_cfg, QStringLiteral("webhook_enabled")) ||
                !l_config->get<bool>(l_cfg, QStringLiteral("webhook_ban_enabled")))
                return;

            BanPayload l_payload;
            l_payload.ban_id = e.ban_id;
            l_payload.moderator = e.moderator;
            l_payload.target_ipid = e.target_ipid;
            l_payload.duration = e.duration;
            l_payload.reason = e.reason;
            m_sender->sendBan(l_payload);
        },
        id());

    l_events->subscribe<akashi::ConfigReloadedEvent>(
        akashi::EventPhase::After, 0,
        [applyConfig](akashi::ConfigReloadedEvent &) { applyConfig(); },
        id());

    return true;
}

void DiscordPlugin::shutdown(akashi::ServiceRegistry &services)
{
    auto l_events = services.resolve<akashi::EventBus>(QStringLiteral("akashi.events"));
    if (l_events)
        l_events->unsubscribeAll(id());

    m_sender.reset();
}
