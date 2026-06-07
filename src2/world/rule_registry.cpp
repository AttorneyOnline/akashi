#include "world/rule_registry.h"

#include <QSet>

namespace akashi {

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

void RuleRegistry::registerBeforeAction(const QString &f_name, BeforeActionFactory f_factory, const QString &f_owner)
{
    m_actions.insert(f_name, {std::move(f_factory), RulePhase::Before, f_owner});
}

void RuleRegistry::registerAfterAction(const QString &f_name, AfterActionFactory f_factory, const QString &f_owner)
{
    m_actions.insert(f_name, {std::move(f_factory), RulePhase::After, f_owner});
}

void RuleRegistry::unregisterActions(const QString &f_owner)
{
    for (auto it = m_actions.begin(); it != m_actions.end();) {
        if (it->owner == f_owner)
            it = m_actions.erase(it);
        else
            ++it;
    }
}

std::optional<BeforeRuleFunction> RuleRegistry::buildBefore(const QString &f_name, ServiceRegistry &f_services, const QVariantMap &f_args) const
{
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
    auto it = m_actions.constFind(f_name);
    if (it == m_actions.constEnd())
        return std::nullopt;
    const auto *l_factory = std::get_if<AfterActionFactory>(&it->factory);
    if (!l_factory)
        return std::nullopt;
    return (*l_factory)(f_services, f_args);
}

RulePhase RuleRegistry::actionPhase(const QString &f_name) const
{
    return m_actions.value(f_name).phase;
}

QStringList RuleRegistry::actionNames() const
{
    return m_actions.keys();
}

QStringList RuleRegistry::actionsOwnedBy(const QString &f_owner) const
{
    QStringList l_result;
    for (auto it = m_actions.constBegin(); it != m_actions.constEnd(); ++it) {
        if (it->owner == f_owner)
            l_result.append(it.key());
    }
    return l_result;
}

bool RuleRegistry::hasAction(const QString &f_name) const
{
    return m_actions.contains(f_name);
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

// --- Introspection ---

QVector<RuleRegistry::RuleInfo> RuleRegistry::rulesForArea(
    const QVector<BeforeRuleEntry> &f_area_before,
    const QVector<AfterRuleEntry> &f_area_after,
    const QVector<BeforeRuleEntry> &f_floor_before,
    const QVector<AfterRuleEntry> &f_floor_after)
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
    for (const auto &e : f_floor_before) {
        if (!l_area_actions.contains(l_key(e.event, e.action)))
            l_result.append({e.event, RulePhase::Before, e.action, e.owner_id, true});
    }
    for (const auto &e : f_floor_after) {
        if (!l_area_actions.contains(l_key(e.event, e.action)))
            l_result.append({e.event, RulePhase::After, e.action, e.owner_id, true});
    }
    return l_result;
}

} // namespace akashi
