#pragma once

namespace akashi {
class CommandRegistry;
class CommandContext;
}

namespace akashi::commands {

// The command handlers this file exports to the registry.
void cmdCoinFlip(akashi::CommandContext &f_context);
void cmdRoll(akashi::CommandContext &f_context);
void cmdRollA(akashi::CommandContext &f_context);
void cmdRollP(akashi::CommandContext &f_context);
void cmdTimer(akashi::CommandContext &f_context);
void cmdNotecard(akashi::CommandContext &f_context);
void cmdNotecardClear(akashi::CommandContext &f_context);
void cmdNotecardReveal(akashi::CommandContext &f_context);
void cmd8Ball(akashi::CommandContext &f_context);
void cmdSubtheme(akashi::CommandContext &f_context);

void registerRoleplayCommands(akashi::CommandRegistry &f_registry);

} // namespace akashi::commands
