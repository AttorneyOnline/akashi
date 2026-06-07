#pragma once

#include "akashi/area_rule.h"
#include "akashi/service.h"
#include "akashi_core_export.h"

#include <QHash>
#include <QString>
#include <QVector>

#include <functional>
#include <memory>
#include <optional>
#include <variant>

namespace akashi {

using BeforeActionFactory = std::function<BeforeRuleFunction(ServiceRegistry &, const QVariantMap &)>;
using AfterActionFactory = std::function<AfterRuleFunction(ServiceRegistry &, const QVariantMap &)>;

// Central registry for rule action definitions and dispatch logic.
//
// Action definitions map a name to a factory that builds a rule function.
// Core registers built-ins at startup; plugins add their own.
//
// Applied rules live on Floor and AreaData objects directly. The static
// dispatch methods here run the override-aware evaluation: area rules
// first, then floor rules whose action name isn't overridden by the area.
class AKASHI_CORE_EXPORT RuleRegistry : public IService
{
  public:
    QString serviceId() const override;
    ServiceVersion serviceVersion() const override;

    // --- Action definitions ---

    void registerBeforeAction(const QString &f_name, BeforeActionFactory f_factory, const QString &f_owner = {});
    void registerAfterAction(const QString &f_name, AfterActionFactory f_factory, const QString &f_owner = {});
    void unregisterActions(const QString &f_owner);

    std::optional<BeforeRuleFunction> buildBefore(const QString &f_name, ServiceRegistry &f_services, const QVariantMap &f_args) const;
    std::optional<AfterRuleFunction> buildAfter(const QString &f_name, ServiceRegistry &f_services, const QVariantMap &f_args) const;

    RulePhase actionPhase(const QString &f_name) const;
    QStringList actionNames() const;
    QStringList actionsOwnedBy(const QString &f_owner) const;
    bool hasAction(const QString &f_name) const;

    // --- Dispatch (reads rules from area/floor objects) ---

    static RuleVerdict checkBefore(const QString &f_event, const RuleContext &f_context,
                                   const QVector<BeforeRuleEntry> &f_area_rules,
                                   const QVector<BeforeRuleEntry> &f_floor_rules);

    static void runAfter(const QString &f_event, const RuleContext &f_context,
                         const QVector<AfterRuleEntry> &f_area_rules,
                         const QVector<AfterRuleEntry> &f_floor_rules);

    // Removes everything an unloading owner leaves behind in one rule
    // store: entries it attached itself and entries built from its actions.
    // Returns how many entries went.
    static int removeRules(const QString &f_owner, const QSet<QString> &f_owned_actions,
                           QVector<BeforeRuleEntry> &f_before, QVector<AfterRuleEntry> &f_after);

    // --- Introspection ---

    struct RuleInfo
    {
        QString event;
        RulePhase phase;
        QString action;
        QString owner_id;
        bool is_floor_rule = false;
    };

    static QVector<RuleInfo> rulesForArea(const QVector<BeforeRuleEntry> &f_area_before,
                                          const QVector<AfterRuleEntry> &f_area_after,
                                          const QVector<BeforeRuleEntry> &f_floor_before,
                                          const QVector<AfterRuleEntry> &f_floor_after);

  private:
    struct ActionFactory
    {
        std::variant<BeforeActionFactory, AfterActionFactory> factory;
        RulePhase phase;
        QString owner;
    };

    QHash<QString, ActionFactory> m_actions;
};

} // namespace akashi
