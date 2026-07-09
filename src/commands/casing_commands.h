#pragma once

namespace akashi {
class CommandContext;
class CommandRegistry;
}

namespace akashi::commands {

// The command handlers this file exports to the registry.
void cmdDoc(akashi::CommandContext &f_context);
void cmdClearDoc(akashi::CommandContext &f_context);
void cmdEvidenceMod(akashi::CommandContext &f_context);
void cmdEvidenceSwap(akashi::CommandContext &f_context);
void cmdTestify(akashi::CommandContext &f_context);
void cmdExamine(akashi::CommandContext &f_context);
void cmdTestimony(akashi::CommandContext &f_context);
void cmdDelete(akashi::CommandContext &f_context);
void cmdUpdate(akashi::CommandContext &f_context);
void cmdPause(akashi::CommandContext &f_context);
void cmdAdd(akashi::CommandContext &f_context);
void cmdSaveTestimony(akashi::CommandContext &f_context);
void cmdLoadTestimony(akashi::CommandContext &f_context);

void registerCasingCommands(akashi::CommandRegistry &f_registry);

} // namespace akashi::commands
