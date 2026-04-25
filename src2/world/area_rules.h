#ifndef WORLD_AREA_RULES_H
#define WORLD_AREA_RULES_H

#include "akashi_core_export.h"

#include <QString>
#include <QVector>

#include <functional>

namespace akashi {

// The moments a rule can watch.
enum class AreaEvent
{
    PlayerJoined,
    PlayerLeft,
    MessageSent,
    EvidencePresented,
};

// What a rule gets to look at when its event happens.
struct AreaEventDetails
{
    int player_id = -1;
    int area_id = -1;
    int floor_id = -1;
    QString text; // the message or evidence name, when the event carries one
};

// What a rule decides about the event itself: let it happen, or block it
// with a reason the player gets to see. A rule's real work may be the
// actions it performs while running - the verdict only gates the event.
struct RuleVerdict
{
    bool allowed = true;
    QString reason;
};

using AreaRuleFunction = std::function<RuleVerdict(const AreaEventDetails &)>;

// Rules automate areas: each area event is a hook, and a rule is a named
// function attached to it. Rules are executors - a rule can act on the
// world when its event fires (say something specific or present a specific
// item, and a rule unlocks a locked area: the function captures what it
// operates on), and it can block the event itself. A server is made up of
// floors, and floors own their areas - a rule added to a floor applies in
// every area under it, while per-area rules are the highest granularity:
// an area rule with the same name overwrites the floor's version there.
// Core, plugins and config files register rules alike (owner-tracked, so a
// plugin unload removes its rules).
class AKASHI_CORE_EXPORT AreaRuleRegistry
{
  public:
    void registerAreaRule(const QString &f_id, AreaEvent f_event, int f_area_id, AreaRuleFunction f_function, const QString &f_owner_id);
    void registerFloorRule(const QString &f_id, AreaEvent f_event, int f_floor_id, AreaRuleFunction f_function, const QString &f_owner_id);
    void unregisterAll(const QString &f_owner_id);

    // Runs every rule attached to this event - the area's own first, then
    // the floor rules the area has not overwritten. All of them execute;
    // the first block decides the event's fate.
    RuleVerdict check(AreaEvent f_event, const AreaEventDetails &f_details) const;

    int ruleCount() const { return m_floor_rules.size() + m_area_rules.size(); }

  private:
    struct Rule
    {
        QString id;
        AreaEvent event;
        int attached_to; // the area or floor the rule watches
        AreaRuleFunction function;
        QString owner_id;
    };

    QVector<Rule> m_floor_rules;
    QVector<Rule> m_area_rules;
};

} // namespace akashi

#endif // WORLD_AREA_RULES_H
