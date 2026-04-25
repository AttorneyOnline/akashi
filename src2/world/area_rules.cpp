#include "world/area_rules.h"

#include <QSet>

namespace akashi {

void AreaRuleRegistry::registerAreaRule(const QString &f_id, AreaEvent f_event, int f_area_id, AreaRuleFunction f_function, const QString &f_owner_id)
{
    m_area_rules.append({f_id, f_event, f_area_id, f_function, f_owner_id});
}

void AreaRuleRegistry::registerFloorRule(const QString &f_id, AreaEvent f_event, int f_floor_id, AreaRuleFunction f_function, const QString &f_owner_id)
{
    m_floor_rules.append({f_id, f_event, f_floor_id, f_function, f_owner_id});
}

// A plugin's rule object rides inside a plain function; the capture keeps
// it alive until the rule is unregistered.
static AreaRuleFunction wrapRule(std::shared_ptr<AreaRule> f_rule)
{
    return [f_rule](const AreaEventDetails &f_details) {
        return f_rule->onEvent(f_details);
    };
}

void AreaRuleRegistry::registerAreaRule(const QString &f_id, AreaEvent f_event, int f_area_id, std::shared_ptr<AreaRule> f_rule, const QString &f_owner_id)
{
    registerAreaRule(f_id, f_event, f_area_id, wrapRule(f_rule), f_owner_id);
}

void AreaRuleRegistry::registerFloorRule(const QString &f_id, AreaEvent f_event, int f_floor_id, std::shared_ptr<AreaRule> f_rule, const QString &f_owner_id)
{
    registerFloorRule(f_id, f_event, f_floor_id, wrapRule(f_rule), f_owner_id);
}

void AreaRuleRegistry::unregisterAll(const QString &f_owner_id)
{
    for (QVector<Rule> *l_rules : {&m_floor_rules, &m_area_rules}) {
        for (int i = l_rules->size() - 1; i >= 0; i--) {
            if (l_rules->at(i).owner_id == f_owner_id) {
                l_rules->removeAt(i);
            }
        }
    }
}

RuleVerdict AreaRuleRegistry::check(AreaEvent f_event, const AreaEventDetails &f_details) const
{
    RuleVerdict l_result;

    // The area's own rules come first - they are the highest granularity -
    // and any floor rule sharing a name with one of them is overwritten.
    // Every attached rule runs (a rule may act, not just judge); the first
    // block is what the event reports.
    QSet<QString> l_overwritten;
    for (const Rule &l_rule : m_area_rules) {
        if (l_rule.event != f_event || l_rule.attached_to != f_details.area_id) {
            continue;
        }
        l_overwritten.insert(l_rule.id);
        const RuleVerdict l_verdict = l_rule.function(f_details);
        if (!l_verdict.allowed && l_result.allowed) {
            l_result = l_verdict;
        }
    }

    // The floor owns its areas, so its remaining rules apply in all of them.
    for (const Rule &l_rule : m_floor_rules) {
        if (l_rule.event != f_event || l_rule.attached_to != f_details.floor_id || l_overwritten.contains(l_rule.id)) {
            continue;
        }
        const RuleVerdict l_verdict = l_rule.function(f_details);
        if (!l_verdict.allowed && l_result.allowed) {
            l_result = l_verdict;
        }
    }
    return l_result;
}

} // namespace akashi
