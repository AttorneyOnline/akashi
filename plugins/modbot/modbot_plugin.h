#pragma once

#include "akashi/plugin.h"
#include "modbot_rules.h"

#include <QObject>
#include <QtPlugin>

#include <memory>

namespace akashi {
class ConfigStore;
class LogService;
class ModerationService;
class PlayerDirectory;
struct RuleContext;
struct TextFilterContext;
} // namespace akashi

namespace akashi::modbot {
class Worker;
}

// The automated moderator: a fast pre-echo screen on the main loop, deep
// analysis on its own worker thread behind a bounded queue, and verdicts
// that act through the same sanction store and ACL roles as human
// moderators. See docs at the roadmap's modbot design tenant.
class ModbotPlugin : public QObject, public akashi::IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID AkashiPlugin_iid FILE "plugin.json")
    Q_INTERFACES(akashi::IPlugin)

  public:
    QString id() const override;
    akashi::ServiceVersion pluginVersion() const override;

    bool load(akashi::ServiceRegistry &services) override;
    void shutdown(akashi::ServiceRegistry &services) override;

  private Q_SLOTS:
    void onVerdict(const akashi::modbot::Verdict &f_verdict);

  private:
    // The inline screen; must stay fast, it runs on the main loop for
    // every message before anyone else hears it.
    std::optional<QString> screenMessage(const QString &f_text, const akashi::TextFilterContext &f_context);

    void observeChatMessage(akashi::modbot::Event::Kind f_kind, const akashi::RuleContext &f_context);
    void observeModcall(const akashi::RuleContext &f_context);
    void observeModeration(akashi::modbot::Event::Kind f_kind, const akashi::RuleContext &f_context);

    // True for people the bot leaves alone: logged-in moderators.
    bool isExempt(int f_client_session_id) const;
    void enqueue(akashi::modbot::Event f_event);
    void notifyModerators(const QString &f_text);
    void warnPlayer(const akashi::modbot::Verdict &f_verdict, const QString &f_text);
    void logAction(const QString &f_ipid, const QString &f_text);

    akashi::modbot::Config m_rules_config;
    QString m_acl_role;
    bool m_notify_moderators = true;
    bool m_exempt_authenticated = true;

    std::shared_ptr<akashi::PlayerDirectory> m_players;
    std::shared_ptr<akashi::ModerationService> m_moderation;
    std::shared_ptr<akashi::LogService> m_log;
    std::unique_ptr<akashi::modbot::Worker> m_worker;
};
