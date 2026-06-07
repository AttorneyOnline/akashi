#include "commands/rule_commands.h"

#include "akashi/permissions.h"
#include "area_data.h"
#include "core/command_context.h"
#include "core/command_registry.h"
#include "server.h"
#include "world/floor.h"
#include "world/rule_registry.h"

namespace akashi::commands {

namespace {

bool isCoreEvent(const QString &f_event)
{
    static const QSet<QString> s_events = {
        AreaEvents::ServerJoined, AreaEvents::PlayerJoined, AreaEvents::PlayerLeft,
        AreaEvents::IcMessageSent, AreaEvents::OocMessageSent,
        AreaEvents::MusicChanged, AreaEvents::AmbienceChanged,
        AreaEvents::EvidencePresented, AreaEvents::EvidenceAdded, AreaEvents::EvidenceRemoved,
        AreaEvents::BackgroundChanged, AreaEvents::LockChanged, AreaEvents::OwnerChanged,
        AreaEvents::CharacterChanged};
    return s_events.contains(f_event);
}

// Turns "message=No talking in court" into {message: "No talking in court"};
// a token without '=' continues the value of the key before it.
QVariantMap parseRuleArgs(const QStringList &f_tokens)
{
    QVariantMap l_args;
    QString l_key;
    for (const QString &l_token : f_tokens) {
        const int l_split = l_token.indexOf(QLatin1Char('='));
        if (l_split > 0) {
            l_key = l_token.left(l_split);
            l_args.insert(l_key, l_token.mid(l_split + 1));
        }
        else if (!l_key.isEmpty()) {
            l_args.insert(l_key, l_args.value(l_key).toString() + QStringLiteral(" ") + l_token);
        }
    }
    return l_args;
}

bool addRuleTo(CommandContext &f_context, const QString &f_event, const QString &f_action, const QVariantMap &f_args,
               QVector<BeforeRuleEntry> &f_before, QVector<AfterRuleEntry> &f_after)
{
    RuleRegistry *l_registry = f_context.server()->ruleRegistry();
    if (!l_registry->hasAction(f_action)) {
        f_context.reply("There is no rule action named " + f_action + ". See /ruleactions.");
        return false;
    }
    if (l_registry->actionPhase(f_action) == RulePhase::Before) {
        if (auto l_function = l_registry->buildBefore(f_action, *f_context.services(), f_args)) {
            f_before.append({f_event, f_action, *l_function, QStringLiteral("command"), f_args});
        }
    }
    else if (auto l_function = l_registry->buildAfter(f_action, *f_context.services(), f_args)) {
        f_after.append({f_event, f_action, *l_function, QStringLiteral("command"), f_args});
    }
    if (!isCoreEvent(f_event)) {
        f_context.reply("Note: " + f_event + " is not a core event; it only fires if a plugin dispatches it.");
    }
    return true;
}

int removeRulesFrom(const QString &f_event, const QString &f_action,
                    QVector<BeforeRuleEntry> &f_before, QVector<AfterRuleEntry> &f_after)
{
    int l_removed = 0;
    const auto l_sweep = [&](auto &f_rules) {
        for (int i = f_rules.size() - 1; i >= 0; --i) {
            if (f_rules.at(i).event == f_event && (f_action.isEmpty() || f_rules.at(i).action == f_action)) {
                f_rules.removeAt(i);
                ++l_removed;
            }
        }
    };
    l_sweep(f_before);
    l_sweep(f_after);
    return l_removed;
}

} // namespace

static void handleRules(CommandContext &f_context)
{
    Server *l_server = f_context.server();
    AreaData *l_area = l_server->areaById(f_context.areaId());
    const Floor *l_floor = l_server->floorById(l_server->floorIdForArea(f_context.areaId()));
    if (!l_area || !l_floor) {
        return;
    }

    const QVector<RuleRegistry::RuleInfo> l_rules = RuleRegistry::rulesForArea(
        l_area->beforeRules(), l_area->afterRules(), l_floor->before_rules, l_floor->after_rules);

    QStringList l_entries;
    l_entries.append("=== Rules in " + f_context.areaName() + " ===");
    for (const RuleRegistry::RuleInfo &l_rule : l_rules) {
        const QString l_scope = l_rule.is_floor_rule ? QStringLiteral("floor") : QStringLiteral("area");
        const QString l_phase = l_rule.phase == RulePhase::Before ? QStringLiteral("before") : QStringLiteral("after");
        l_entries.append(l_rule.event + " [" + l_phase + "] " + l_rule.action + " (" + l_scope + ", " + l_rule.owner_id + ")");
    }
    if (l_rules.isEmpty()) {
        l_entries.append("No rules apply here.");
    }
    f_context.reply(l_entries.join("\n"));
}

static void handleRuleActions(CommandContext &f_context)
{
    RuleRegistry *l_registry = f_context.server()->ruleRegistry();
    QStringList l_names = l_registry->actionNames();
    std::sort(l_names.begin(), l_names.end());

    QStringList l_entries;
    l_entries.append("=== Rule actions ===");
    for (const QString &l_name : l_names) {
        const QString l_phase = l_registry->actionPhase(l_name) == RulePhase::Before ? QStringLiteral("before") : QStringLiteral("after");
        l_entries.append(l_name + " (" + l_phase + ")");
    }
    f_context.reply(l_entries.join("\n"));
}

static void handleAddRule(CommandContext &f_context)
{
    AreaData *l_area = f_context.server()->areaById(f_context.areaId());
    if (!l_area) {
        return;
    }
    const QString l_event = f_context.argument(0);
    const QString l_action = f_context.argument(1);
    if (addRuleTo(f_context, l_event, l_action, parseRuleArgs(f_context.arguments().mid(2)),
                  l_area->beforeRules(), l_area->afterRules())) {
        f_context.reply("Added " + l_action + " to " + l_event + " in this area.");
    }
}

static void handleRemoveRule(CommandContext &f_context)
{
    AreaData *l_area = f_context.server()->areaById(f_context.areaId());
    if (!l_area) {
        return;
    }
    const QString l_event = f_context.argument(0);
    const QString l_action = f_context.argument(1);
    const int l_removed = removeRulesFrom(l_event, l_action, l_area->beforeRules(), l_area->afterRules());
    f_context.reply(l_removed > 0
                        ? "Removed " + QString::number(l_removed) + " rule(s) from this area."
                        : "This area has no matching rules. Floor rules are removed with /floorrule.");
}

static void handleFloorRule(CommandContext &f_context)
{
    Floor *l_floor = f_context.server()->floorById(f_context.server()->floorIdForArea(f_context.areaId()));
    if (!l_floor) {
        return;
    }
    const QString l_mode = f_context.argument(0);
    const QString l_event = f_context.argument(1);
    if (l_mode == QStringLiteral("add")) {
        const QString l_action = f_context.argument(2);
        if (l_action.isEmpty()) {
            f_context.reply("Usage: /floorrule add <event> <action> [key=value ...]");
            return;
        }
        if (addRuleTo(f_context, l_event, l_action, parseRuleArgs(f_context.arguments().mid(3)),
                      l_floor->before_rules, l_floor->after_rules)) {
            f_context.reply("Added " + l_action + " to " + l_event + " on floor " + l_floor->name + ".");
        }
    }
    else if (l_mode == QStringLiteral("remove")) {
        const int l_removed = removeRulesFrom(l_event, f_context.argument(2), l_floor->before_rules, l_floor->after_rules);
        f_context.reply(l_removed > 0
                            ? "Removed " + QString::number(l_removed) + " rule(s) from floor " + l_floor->name + "."
                            : "This floor has no matching rules.");
    }
    else {
        f_context.reply("Usage: /floorrule <add|remove> <event> [action] [key=value ...]");
    }
}

static void handleReloadRules(CommandContext &f_context)
{
    f_context.server()->applyConfigRules();
    f_context.reply("Reapplied the rule declarations from areas.json.");
}

void registerRuleCommands(CommandRegistry &f_registry)
{
    f_registry.registerCommand(
        {QStringLiteral("rules"), {}, {}, 0,
         QStringLiteral("/rules"),
         QStringLiteral("Lists the rules active in this area, floor rules included.")},
        handleRules, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("ruleactions"), {}, {}, 0,
         QStringLiteral("/ruleactions"),
         QStringLiteral("Lists the rule actions available to /addrule.")},
        handleRuleActions, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("addrule"), {}, {akashi::permission::modify_rules}, 2,
         QStringLiteral("/addrule <event> <action> [key=value ...]"),
         QStringLiteral("Attaches a rule action to an event in this area.")},
        handleAddRule, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("removerule"), {}, {akashi::permission::modify_rules}, 1,
         QStringLiteral("/removerule <event> [action]"),
         QStringLiteral("Removes this area's rules for an event.")},
        handleRemoveRule, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("floorrule"), {}, {akashi::permission::modify_floors}, 2,
         QStringLiteral("/floorrule <add|remove> <event> [action] [key=value ...]"),
         QStringLiteral("Adds or removes a rule on this floor.")},
        handleFloorRule, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("reloadrules"), {}, {akashi::permission::modify_floors}, 0,
         QStringLiteral("/reloadrules"),
         QStringLiteral("Reapplies the rule declarations from areas.json.")},
        handleReloadRules, QStringLiteral("core"));
}

} // namespace akashi::commands
