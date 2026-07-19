#include "commands/rule_commands.h"

#include "akashi/log_event.h"
#include "akashi/permissions.h"
#include "core/command_context.h"
#include "core/command_registry.h"
#include "core/log_service.h"
#include "core/server_context.h"
#include "world/area.h"
#include "world/floor.h"
#include "world/rule_registry.h"

namespace akashi::commands {

namespace {

bool isCoreEvent(const QString &f_event)
{
    return RuleRegistry::eventSupportsPhase(f_event, RulePhase::Before).has_value();
}

QString phaseName(RulePhase f_phase)
{
    switch (f_phase) {
    case RulePhase::Before:
        return QStringLiteral("before");
    case RulePhase::Transform:
        return QStringLiteral("transform");
    case RulePhase::After:
        return QStringLiteral("after");
    }
    return QStringLiteral("before");
}

// Rule mutations are moderator actions; the audit line names who did it.
void logRuleChange(CommandContext &f_context, const QString &f_command, const QString &f_description)
{
    QString l_actor = f_context.moderatorName();
    if (l_actor.isEmpty())
        l_actor = f_context.character();
    if (l_actor.isEmpty())
        l_actor = f_context.name();
    f_context.server()->logService()->log({.type = log_type::CMD,
                                           .area = f_context.areaName(),
                                           .char_name = f_context.character(),
                                           .ooc_name = f_context.name(),
                                           .ipid = f_context.ipid(),
                                           .message = f_command,
                                           .args = f_description,
                                           .moderator = l_actor});
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
               QVector<BeforeRuleEntry> &f_before, QVector<AfterRuleEntry> &f_after,
               QVector<TransformRuleEntry> &f_transforms)
{
    RuleRegistry *l_registry = f_context.server()->ruleRegistry();
    if (!l_registry->hasAction(f_action)) {
        f_context.reply("There is no rule action named " + f_action + ". See /ruleactions.");
        return false;
    }
    const RulePhase l_phase = l_registry->actionPhase(f_action);
    const std::optional<bool> l_supported = RuleRegistry::eventSupportsPhase(f_event, l_phase);
    if (l_supported.has_value() && !*l_supported) {
        f_context.reply(f_action + " is a " + phaseName(l_phase) + " action, but " + f_event + " does not dispatch " + phaseName(l_phase) + " rules.");
        return false;
    }
    if (l_phase == RulePhase::Before) {
        if (auto l_function = l_registry->buildBefore(f_action, *f_context.services(), f_args)) {
            f_before.append({f_event, f_action, *l_function, QStringLiteral("command"), f_args});
        }
    }
    else if (l_phase == RulePhase::Transform) {
        if (auto l_function = l_registry->buildTransform(f_action, *f_context.services(), f_args)) {
            f_transforms.append({f_event, f_action, *l_function, QStringLiteral("command"), f_args});
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
                    QVector<BeforeRuleEntry> &f_before, QVector<AfterRuleEntry> &f_after,
                    QVector<TransformRuleEntry> &f_transforms)
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
    l_sweep(f_transforms);
    return l_removed;
}

} // namespace

void cmdRules(CommandContext &f_context)
{
    ServerContext *l_server = f_context.server();
    akashi::Area *l_area = l_server->areaById(f_context.areaId());
    const Floor *l_floor = l_server->floorById(l_server->floorIdForArea(f_context.areaId()));
    if (!l_area || !l_floor) {
        return;
    }

    const QVector<RuleRegistry::RuleInfo> l_rules = RuleRegistry::rulesForArea(
        l_area->beforeRules(), l_area->afterRules(), l_area->transformRules(),
        l_floor->before_rules, l_floor->after_rules, l_floor->transform_rules);

    QStringList l_entries;
    l_entries.append("=== Rules in " + f_context.areaName() + " ===");
    for (const RuleRegistry::RuleInfo &l_rule : l_rules) {
        const QString l_scope = l_rule.is_floor_rule ? QStringLiteral("floor") : QStringLiteral("area");
        l_entries.append(l_rule.event + " [" + phaseName(l_rule.phase) + "] " + l_rule.action + " (" + l_scope + ", " + l_rule.owner_id + ")");
    }
    if (l_rules.isEmpty()) {
        l_entries.append("No rules apply here.");
    }
    f_context.reply(l_entries.join("\n"));
}

void cmdRuleActions(CommandContext &f_context)
{
    RuleRegistry *l_registry = f_context.server()->ruleRegistry();
    QStringList l_names = l_registry->actionNames();
    std::sort(l_names.begin(), l_names.end());

    QStringList l_entries;
    l_entries.append("=== Rule actions ===");
    for (const QString &l_name : l_names) {
        l_entries.append(l_name + " (" + phaseName(l_registry->actionPhase(l_name)) + ")");
    }
    f_context.reply(l_entries.join("\n"));
}

void cmdAddRule(CommandContext &f_context)
{
    akashi::Area *l_area = f_context.server()->areaById(f_context.areaId());
    if (!l_area) {
        return;
    }
    const QString l_event = f_context.argument(0);
    const QString l_action = f_context.argument(1);
    if (addRuleTo(f_context, l_event, l_action, parseRuleArgs(f_context.arguments().mid(2)),
                  l_area->beforeRules(), l_area->afterRules(), l_area->transformRules())) {
        f_context.reply("Added " + l_action + " to " + l_event + " in this area.");
        logRuleChange(f_context, QStringLiteral("addrule"), "attached " + l_action + " to " + l_event + " in area " + f_context.areaName());
    }
}

void cmdRemoveRule(CommandContext &f_context)
{
    akashi::Area *l_area = f_context.server()->areaById(f_context.areaId());
    if (!l_area) {
        return;
    }
    const QString l_event = f_context.argument(0);
    const QString l_action = f_context.argument(1);
    const int l_removed = removeRulesFrom(l_event, l_action, l_area->beforeRules(), l_area->afterRules(), l_area->transformRules());
    f_context.reply(l_removed > 0
                        ? "Removed " + QString::number(l_removed) + " rule(s) from this area."
                        : "This area has no matching rules. Floor rules are removed with /floorrule.");
    if (l_removed > 0) {
        const QString l_what = l_action.isEmpty() ? l_event : l_event + " " + l_action;
        logRuleChange(f_context, QStringLiteral("removerule"), "removed " + QString::number(l_removed) + " " + l_what + " rule(s) from area " + f_context.areaName());
    }
}

void cmdFloorRule(CommandContext &f_context)
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
                      l_floor->before_rules, l_floor->after_rules, l_floor->transform_rules)) {
            f_context.reply("Added " + l_action + " to " + l_event + " on floor " + l_floor->name + ".");
            logRuleChange(f_context, QStringLiteral("floorrule"), "attached " + l_action + " to " + l_event + " on floor " + l_floor->name);
        }
    }
    else if (l_mode == QStringLiteral("remove")) {
        const QString l_action = f_context.argument(2);
        const int l_removed = removeRulesFrom(l_event, l_action, l_floor->before_rules, l_floor->after_rules, l_floor->transform_rules);
        f_context.reply(l_removed > 0
                            ? "Removed " + QString::number(l_removed) + " rule(s) from floor " + l_floor->name + "."
                            : "This floor has no matching rules.");
        if (l_removed > 0) {
            const QString l_what = l_action.isEmpty() ? l_event : l_event + " " + l_action;
            logRuleChange(f_context, QStringLiteral("floorrule"), "removed " + QString::number(l_removed) + " " + l_what + " rule(s) from floor " + l_floor->name);
        }
    }
    else {
        f_context.reply("Usage: /floorrule <add|remove> <event> [action] [key=value ...]");
    }
}

void cmdReloadRules(CommandContext &f_context)
{
    f_context.server()->applyConfigRules();
    f_context.server()->applyConfigGrants();
    f_context.reply("Reapplied the rule and grant declarations from areas.json.");
    logRuleChange(f_context, QStringLiteral("reloadrules"), QStringLiteral("reapplied the rule and grant declarations from areas.json"));
}

void registerRuleCommands(CommandRegistry &f_registry)
{
    f_registry.registerCommand(
        {QStringLiteral("rules"), {}, {akashi::permission::info_rules}, 0, QStringLiteral("/rules"), QStringLiteral("Lists the rules active in this area, floor rules included.")},
        cmdRules, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("ruleactions"), {}, {akashi::permission::info_ruleactions}, 0, QStringLiteral("/ruleactions"), QStringLiteral("Lists the rule actions available to /addrule.")},
        cmdRuleActions, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("addrule"), {}, {akashi::permission::modify_rules}, 2, QStringLiteral("/addrule <event> <action> [key=value ...]"), QStringLiteral("Attaches a rule action to an event in this area.")},
        cmdAddRule, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("removerule"), {}, {akashi::permission::modify_rules}, 1, QStringLiteral("/removerule <event> [action]"), QStringLiteral("Removes this area's rules for an event.")},
        cmdRemoveRule, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("floorrule"), {}, {akashi::permission::modify_floors}, 2, QStringLiteral("/floorrule <add|remove> <event> [action] [key=value ...]"), QStringLiteral("Adds or removes a rule on this floor.")},
        cmdFloorRule, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("reloadrules"), {}, {akashi::permission::modify_floors}, 0, QStringLiteral("/reloadrules"), QStringLiteral("Reapplies the rule declarations from areas.json.")},
        cmdReloadRules, QStringLiteral("core"));
}

} // namespace akashi::commands
