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

class QThread;

namespace akashi {

using BeforeActionFactory = std::function<BeforeRuleFunction(ServiceRegistry &, const QVariantMap &)>;
using AfterActionFactory = std::function<AfterRuleFunction(ServiceRegistry &, const QVariantMap &)>;
using TransformActionFactory = std::function<TransformRuleFunction(ServiceRegistry &, const QVariantMap &)>;

// Central registry for rule action definitions and dispatch logic.
//
// Action definitions map a name to a factory that builds a rule function.
// Core registers built-ins at startup; plugins add their own.
//
// Applied rules live on Floor and Area objects directly. The static
// dispatch methods here run the override-aware evaluation: area rules
// first, then floor rules whose action name isn't overridden by the area.
//
// The registry also holds the global observers: bare functions watching
// an event after it committed, ordered by an explicit order value.
class AKASHI_CORE_EXPORT RuleRegistry : public IService
{
  public:
    RuleRegistry();

    QString serviceId() const override;
    ServiceVersion serviceVersion() const override;

    // --- Action definitions ---

    // A name registers once; a second registration is refused with a
    // warning and the first owner keeps the definition.
    void registerBeforeAction(const QString &f_name, BeforeActionFactory f_factory, const QString &f_owner = {});
    void registerAfterAction(const QString &f_name, AfterActionFactory f_factory, const QString &f_owner = {});
    void registerTransformAction(const QString &f_name, TransformActionFactory f_factory, const QString &f_owner = {});
    void unregisterActions(const QString &f_owner);

    std::optional<BeforeRuleFunction> buildBefore(const QString &f_name, ServiceRegistry &f_services, const QVariantMap &f_args) const;
    std::optional<AfterRuleFunction> buildAfter(const QString &f_name, ServiceRegistry &f_services, const QVariantMap &f_args) const;
    std::optional<TransformRuleFunction> buildTransform(const QString &f_name, ServiceRegistry &f_services, const QVariantMap &f_args) const;

    RulePhase actionPhase(const QString &f_name) const;
    QStringList actionNames() const;
    QStringList actionsOwnedBy(const QString &f_owner) const;
    bool hasAction(const QString &f_name) const;

    // Whether a core catalog event dispatches the given phase. Ids not
    // in the catalog are plugin/custom events and return nothing.
    static std::optional<bool> eventSupportsPhase(const QString &f_event, RulePhase f_phase);

    // --- Dispatch (reads rules from area/floor objects) ---

    static RuleVerdict checkBefore(const QString &f_event, const RuleContext &f_context,
                                   const QVector<BeforeRuleEntry> &f_area_rules,
                                   const QVector<BeforeRuleEntry> &f_floor_rules);

    // Runs the transforms between gate and commit: area entries first
    // (suppressing same-named floor actions), each entry's returned keys
    // merged into a working payload the later entries see. Returns the
    // final payload for the verb to commit.
    static QVariantMap runTransforms(const QString &f_event, const RuleContext &f_context,
                                     const QVector<TransformRuleEntry> &f_area_rules,
                                     const QVector<TransformRuleEntry> &f_floor_rules);

    static void runAfter(const QString &f_event, const RuleContext &f_context,
                         const QVector<AfterRuleEntry> &f_area_rules,
                         const QVector<AfterRuleEntry> &f_floor_rules);

    // Removes everything an unloading owner leaves behind in one rule
    // store: entries it attached itself and entries built from its actions.
    // Returns how many entries went.
    static int removeRules(const QString &f_owner, const QSet<QString> &f_owned_actions,
                           QVector<BeforeRuleEntry> &f_before, QVector<AfterRuleEntry> &f_after);

    // Same sweep for a store that also holds transform rules.
    static int removeRules(const QString &f_owner, const QSet<QString> &f_owned_actions,
                           QVector<BeforeRuleEntry> &f_before, QVector<AfterRuleEntry> &f_after,
                           QVector<TransformRuleEntry> &f_transforms);

    // --- Observers (global, post-commit) ---

    // Observers run in ascending order; ties keep registration order.
    // Main thread only, like everything else here.
    void registerObserver(const QString &f_event, int f_order, ObserverFunction f_fn, const QString &f_owner = {});
    void unregisterObservers(const QString &f_owner);
    void notifyObservers(const QString &f_event, const RuleContext &f_context);

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
                                          const QVector<TransformRuleEntry> &f_area_transforms,
                                          const QVector<BeforeRuleEntry> &f_floor_before,
                                          const QVector<AfterRuleEntry> &f_floor_after,
                                          const QVector<TransformRuleEntry> &f_floor_transforms);

  private:
    bool refuseTakenActionName(const QString &f_name, const QString &f_owner) const;

    struct ActionFactory
    {
        std::variant<BeforeActionFactory, AfterActionFactory, TransformActionFactory> factory;
        RulePhase phase;
        QString owner;
    };

    struct ObserverEntry
    {
        QString event;
        int order = 0;
        ObserverFunction function;
        QString owner;
    };

    QHash<QString, ActionFactory> m_actions;
    QVector<ObserverEntry> m_observers;
    QThread *m_owner_thread;
};

} // namespace akashi
