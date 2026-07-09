#pragma once

namespace akashi {
class CommandContext;
class CommandRegistry;
}

namespace akashi::commands {

// The command handlers this file exports to the registry.
void cmdRules(akashi::CommandContext &f_context);
void cmdRuleActions(akashi::CommandContext &f_context);
void cmdAddRule(akashi::CommandContext &f_context);
void cmdRemoveRule(akashi::CommandContext &f_context);
void cmdFloorRule(akashi::CommandContext &f_context);
void cmdReloadRules(akashi::CommandContext &f_context);

void registerRuleCommands(akashi::CommandRegistry &f_registry);

} // namespace akashi::commands
