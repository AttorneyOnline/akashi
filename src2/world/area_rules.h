#ifndef WORLD_AREA_RULES_H
#define WORLD_AREA_RULES_H

#include "akashi/area_rule.h"
#include "akashi_core_export.h"

#include <QString>
#include <QVector>

#include <functional>
#include <memory>

namespace akashi {

using AreaRuleFunction = std::function<RuleVerdict(const AreaEventDetails &)>;

// Rules automate areas: each area event is a hook, and a rule is a named
// function attached to it. Rules are executors - a rule can act on the
// world when its event fires (say something specific or present a specific
// item, and a rule unlocks a locked area: the function captures what it
// operates on), and it can block the event itself. A server is made up of
// floors, and floors own their areas - a rule added to a floor applies in
// every area under it, while per-area rules are the highest granularity:
// an area rule with the same name overwrites the floor's version there.
//
// Core and config files attach plain functions; plugins ship AreaRule
// objects (the stable contract in akashi/area_rule.h) and register them
// under their owner id, so a plugin unload removes and releases its rules.
class AKASHI_CORE_EXPORT AreaRuleRegistry
{
  public:
    void registerAreaRule(const QString &f_id, AreaEvent f_event, RulePhase f_phase, int f_area_id, AreaRuleFunction f_function, const QString &f_owner_id);
    void registerFloorRule(const QString &f_id, AreaEvent f_event, RulePhase f_phase, int f_floor_id, AreaRuleFunction f_function, const QString &f_owner_id);

    // The plugin path: the registry shares ownership of the rule object
    // until it is unregistered.
    void registerAreaRule(const QString &f_id, AreaEvent f_event, RulePhase f_phase, int f_area_id, std::shared_ptr<AreaRule> f_rule, const QString &f_owner_id);
    void registerFloorRule(const QString &f_id, AreaEvent f_event, RulePhase f_phase, int f_floor_id, std::shared_ptr<AreaRule> f_rule, const QString &f_owner_id);

    void unregisterAll(const QString &f_owner_id);

    // Runs every rule of one phase attached to this event - the area's own
    // first, then the floor rules the area has not overwritten. All of them
    // execute; the first block decides. The world checks Before ahead of
    // applying a change and only runs After once the change is real.
    RuleVerdict check(AreaEvent f_event, RulePhase f_phase, const AreaEventDetails &f_details) const;

    int ruleCount() const { return m_floor_rules.size() + m_area_rules.size(); }

  private:
    struct Rule
    {
        QString id;
        AreaEvent event;
        RulePhase phase;
        int attached_to; // the area or floor the rule watches
        AreaRuleFunction function;
        QString owner_id;
    };

    QVector<Rule> m_floor_rules;
    QVector<Rule> m_area_rules;
};

} // namespace akashi

#endif // WORLD_AREA_RULES_H
