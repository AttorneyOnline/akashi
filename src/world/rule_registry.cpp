#include "world/rule_registry.h"

#include "akashi/logging_categories.h"
#include "akashi/thread_assert.h"

#include <QDebug>
#include <QSet>

#include <algorithm>
#include <utility>

namespace akashi {

RuleRegistry::RuleRegistry() :
    m_owner_thread(QThread::currentThread())
{}

// --- IService ---

QString RuleRegistry::serviceId() const
{
    return QStringLiteral("akashi.rules");
}

ServiceVersion RuleRegistry::serviceVersion() const
{
    return {1, 0, 0};
}

// --- Action definitions ---

// A name stays with its first owner. Overwriting would let a later plugin
// capture a core action while the name still sits in the first owner's
// unload sweep, so a duplicate is refused with a warning instead.
bool RuleRegistry::refuseTakenActionName(const QString &f_name, const QString &f_owner) const
{
    auto it = m_actions.constFind(f_name);
    if (it == m_actions.constEnd())
        return false;
    qCWarning(akashiServer) << "Refused rule action" << f_name << "from" << f_owner
                            << "- the name is already registered by" << it->owner;
    return true;
}

void RuleRegistry::registerBeforeAction(const QString &f_name, BeforeActionFactory f_factory, const QString &f_owner)
{
    AKASHI_ASSERT_OWNER_THREAD();
    if (refuseTakenActionName(f_name, f_owner))
        return;
    m_actions.insert(f_name, {std::move(f_factory), RulePhase::Before, f_owner});
}

void RuleRegistry::registerAfterAction(const QString &f_name, AfterActionFactory f_factory, const QString &f_owner)
{
    AKASHI_ASSERT_OWNER_THREAD();
    if (refuseTakenActionName(f_name, f_owner))
        return;
    m_actions.insert(f_name, {std::move(f_factory), RulePhase::After, f_owner});
}

void RuleRegistry::registerTransformAction(const QString &f_name, TransformActionFactory f_factory, const QString &f_owner)
{
    AKASHI_ASSERT_OWNER_THREAD();
    if (refuseTakenActionName(f_name, f_owner))
        return;
    m_actions.insert(f_name, {std::move(f_factory), RulePhase::Transform, f_owner});
}

void RuleRegistry::unregisterActions(const QString &f_owner)
{
    AKASHI_ASSERT_OWNER_THREAD();
    m_actions.removeIf([&f_owner](std::pair<const QString &, ActionFactory &> f_item) {
        return f_item.second.owner == f_owner;
    });
}

std::optional<BeforeRuleFunction> RuleRegistry::buildBefore(const QString &f_name, ServiceRegistry &f_services, const QVariantMap &f_args) const
{
    AKASHI_ASSERT_OWNER_THREAD();
    auto it = m_actions.constFind(f_name);
    if (it == m_actions.constEnd())
        return std::nullopt;
    const auto *l_factory = std::get_if<BeforeActionFactory>(&it->factory);
    if (!l_factory)
        return std::nullopt;
    return (*l_factory)(f_services, f_args);
}

std::optional<AfterRuleFunction> RuleRegistry::buildAfter(const QString &f_name, ServiceRegistry &f_services, const QVariantMap &f_args) const
{
    AKASHI_ASSERT_OWNER_THREAD();
    auto it = m_actions.constFind(f_name);
    if (it == m_actions.constEnd())
        return std::nullopt;
    const auto *l_factory = std::get_if<AfterActionFactory>(&it->factory);
    if (!l_factory)
        return std::nullopt;
    return (*l_factory)(f_services, f_args);
}

std::optional<TransformRuleFunction> RuleRegistry::buildTransform(const QString &f_name, ServiceRegistry &f_services, const QVariantMap &f_args) const
{
    AKASHI_ASSERT_OWNER_THREAD();
    auto it = m_actions.constFind(f_name);
    if (it == m_actions.constEnd())
        return std::nullopt;
    const auto *l_factory = std::get_if<TransformActionFactory>(&it->factory);
    if (!l_factory)
        return std::nullopt;
    return (*l_factory)(f_services, f_args);
}

RulePhase RuleRegistry::actionPhase(const QString &f_name) const
{
    AKASHI_ASSERT_OWNER_THREAD();
    return m_actions.value(f_name).phase;
}

QStringList RuleRegistry::actionNames() const
{
    AKASHI_ASSERT_OWNER_THREAD();
    return m_actions.keys();
}

QStringList RuleRegistry::actionsOwnedBy(const QString &f_owner) const
{
    AKASHI_ASSERT_OWNER_THREAD();
    QStringList l_result;
    for (auto it = m_actions.constBegin(); it != m_actions.constEnd(); ++it) {
        if (it->owner == f_owner)
            l_result.append(it.key());
    }
    return l_result;
}

bool RuleRegistry::hasAction(const QString &f_name) const
{
    AKASHI_ASSERT_OWNER_THREAD();
    return m_actions.contains(f_name);
}

std::optional<bool> RuleRegistry::eventSupportsPhase(const QString &f_event, RulePhase f_phase)
{
    for (const AreaEventInfo &l_info : areaEventCatalog()) {
        if (l_info.id != f_event)
            continue;
        switch (f_phase) {
        case RulePhase::Before:
            return l_info.supports_before;
        case RulePhase::After:
            return l_info.supports_after;
        case RulePhase::Transform:
            return l_info.supports_transform;
        }
    }
    return std::nullopt;
}

// --- Dispatch ---

RuleVerdict RuleRegistry::checkBefore(const QString &f_event, const RuleContext &f_context,
                                      const QVector<BeforeRuleEntry> &f_area_rules,
                                      const QVector<BeforeRuleEntry> &f_floor_rules)
{
    RuleVerdict l_result;

    QSet<QString> l_area_actions;
    for (const BeforeRuleEntry &l_entry : f_area_rules) {
        if (l_entry.event != f_event)
            continue;
        l_area_actions.insert(l_entry.action);
        const RuleVerdict l_verdict = l_entry.function(f_context);
        if (!l_verdict.allowed && l_result.allowed)
            l_result = l_verdict;
    }

    for (const BeforeRuleEntry &l_entry : f_floor_rules) {
        if (l_entry.event != f_event)
            continue;
        if (l_area_actions.contains(l_entry.action))
            continue;
        const RuleVerdict l_verdict = l_entry.function(f_context);
        if (!l_verdict.allowed && l_result.allowed)
            l_result = l_verdict;
    }

    return l_result;
}

QVariantMap RuleRegistry::runTransforms(const QString &f_event, const RuleContext &f_context,
                                        const QVector<TransformRuleEntry> &f_area_rules,
                                        const QVector<TransformRuleEntry> &f_floor_rules)
{
    // Each transform sees the payload as its predecessors left it.
    RuleContext l_working = f_context;

    const auto l_apply = [&l_working](const TransformRuleEntry &f_entry) {
        const QVariantMap l_changes = f_entry.function(l_working);
        for (auto it = l_changes.constBegin(); it != l_changes.constEnd(); ++it)
            l_working.payload.insert(it.key(), it.value());
    };

    QSet<QString> l_area_actions;
    for (const TransformRuleEntry &l_entry : f_area_rules) {
        if (l_entry.event != f_event)
            continue;
        l_area_actions.insert(l_entry.action);
        l_apply(l_entry);
    }

    for (const TransformRuleEntry &l_entry : f_floor_rules) {
        if (l_entry.event != f_event)
            continue;
        if (l_area_actions.contains(l_entry.action))
            continue;
        l_apply(l_entry);
    }

    return l_working.payload;
}

void RuleRegistry::runAfter(const QString &f_event, const RuleContext &f_context,
                            const QVector<AfterRuleEntry> &f_area_rules,
                            const QVector<AfterRuleEntry> &f_floor_rules)
{
    QSet<QString> l_area_actions;
    for (const AfterRuleEntry &l_entry : f_area_rules) {
        if (l_entry.event != f_event)
            continue;
        l_area_actions.insert(l_entry.action);
        l_entry.function(f_context);
    }

    for (const AfterRuleEntry &l_entry : f_floor_rules) {
        if (l_entry.event != f_event)
            continue;
        if (l_area_actions.contains(l_entry.action))
            continue;
        l_entry.function(f_context);
    }
}

int RuleRegistry::removeRules(const QString &f_owner, const QSet<QString> &f_owned_actions,
                              QVector<BeforeRuleEntry> &f_before, QVector<AfterRuleEntry> &f_after)
{
    int l_removed = 0;
    const auto l_sweep = [&](auto &f_rules) {
        for (int i = f_rules.size() - 1; i >= 0; --i) {
            if (f_rules.at(i).owner_id == f_owner || f_owned_actions.contains(f_rules.at(i).action)) {
                f_rules.removeAt(i);
                ++l_removed;
            }
        }
    };
    l_sweep(f_before);
    l_sweep(f_after);
    return l_removed;
}

int RuleRegistry::removeRules(const QString &f_owner, const QSet<QString> &f_owned_actions,
                              QVector<BeforeRuleEntry> &f_before, QVector<AfterRuleEntry> &f_after,
                              QVector<TransformRuleEntry> &f_transforms)
{
    int l_removed = removeRules(f_owner, f_owned_actions, f_before, f_after);
    for (int i = f_transforms.size() - 1; i >= 0; --i) {
        if (f_transforms.at(i).owner_id == f_owner || f_owned_actions.contains(f_transforms.at(i).action)) {
            f_transforms.removeAt(i);
            ++l_removed;
        }
    }
    return l_removed;
}

// --- Observers ---

void RuleRegistry::registerObserver(const QString &f_event, int f_order, ObserverFunction f_fn, const QString &f_owner)
{
    AKASHI_ASSERT_OWNER_THREAD();
    // Insert after any equal orders, so ties keep registration order.
    auto l_it = std::upper_bound(m_observers.cbegin(), m_observers.cend(), f_order,
                                 [](int f_value, const ObserverEntry &f_entry) { return f_value < f_entry.order; });
    m_observers.insert(l_it, {f_event, f_order, std::move(f_fn), f_owner});
}

void RuleRegistry::unregisterObservers(const QString &f_owner)
{
    AKASHI_ASSERT_OWNER_THREAD();
    m_observers.removeIf([&f_owner](const ObserverEntry &f_entry) { return f_entry.owner == f_owner; });
}

void RuleRegistry::notifyObservers(const QString &f_event, const RuleContext &f_context)
{
    AKASHI_ASSERT_OWNER_THREAD();
    // Iterate a snapshot so an observer may register or unregister an observer
    // from inside its own callback (or a plugin may unload mid-dispatch)
    // without invalidating the vector under us; the change takes effect from
    // the next event on.
    const QVector<ObserverEntry> l_observers = m_observers;
    for (const ObserverEntry &l_entry : l_observers) {
        if (l_entry.event == f_event)
            l_entry.function(f_context);
    }
}

// --- Introspection ---

QVector<RuleRegistry::RuleInfo> RuleRegistry::rulesForArea(
    const QVector<BeforeRuleEntry> &f_area_before,
    const QVector<AfterRuleEntry> &f_area_after,
    const QVector<TransformRuleEntry> &f_area_transforms,
    const QVector<BeforeRuleEntry> &f_floor_before,
    const QVector<AfterRuleEntry> &f_floor_after,
    const QVector<TransformRuleEntry> &f_floor_transforms)
{
    QVector<RuleInfo> l_result;
    QSet<QString> l_area_actions;

    auto l_key = [](const QString &f_event, const QString &f_action) {
        return f_event + QChar(':') + f_action;
    };

    for (const auto &e : f_area_before) {
        l_result.append({e.event, RulePhase::Before, e.action, e.owner_id, false});
        l_area_actions.insert(l_key(e.event, e.action));
    }
    for (const auto &e : f_area_after) {
        l_result.append({e.event, RulePhase::After, e.action, e.owner_id, false});
        l_area_actions.insert(l_key(e.event, e.action));
    }
    for (const auto &e : f_area_transforms) {
        l_result.append({e.event, RulePhase::Transform, e.action, e.owner_id, false});
        l_area_actions.insert(l_key(e.event, e.action));
    }
    for (const auto &e : f_floor_before) {
        if (!l_area_actions.contains(l_key(e.event, e.action)))
            l_result.append({e.event, RulePhase::Before, e.action, e.owner_id, true});
    }
    for (const auto &e : f_floor_after) {
        if (!l_area_actions.contains(l_key(e.event, e.action)))
            l_result.append({e.event, RulePhase::After, e.action, e.owner_id, true});
    }
    for (const auto &e : f_floor_transforms) {
        if (!l_area_actions.contains(l_key(e.event, e.action)))
            l_result.append({e.event, RulePhase::Transform, e.action, e.owner_id, true});
    }
    return l_result;
}

} // namespace akashi
