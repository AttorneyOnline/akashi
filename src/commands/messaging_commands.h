#pragma once

namespace akashi {
class CommandRegistry;
class CommandContext;
}

namespace akashi::commands {

// The command handlers this file exports to the registry.
void cmdPos(akashi::CommandContext &f_context);
void cmdForcePos(akashi::CommandContext &f_context);
void cmdG(akashi::CommandContext &f_context);
void cmdNeed(akashi::CommandContext &f_context);
void cmdSwitch(akashi::CommandContext &f_context);
void cmdRandomChar(akashi::CommandContext &f_context);
void cmdToggleGlobal(akashi::CommandContext &f_context);
void cmdPM(akashi::CommandContext &f_context);
void cmdAnnounce(akashi::CommandContext &f_context);
void cmdM(akashi::CommandContext &f_context);
void cmdGM(akashi::CommandContext &f_context);
void cmdLM(akashi::CommandContext &f_context);
void cmdGimp(akashi::CommandContext &f_context);
void cmdUngimp(akashi::CommandContext &f_context);
void cmdDisemvowel(akashi::CommandContext &f_context);
void cmdUnDisemvowel(akashi::CommandContext &f_context);
void cmdShake(akashi::CommandContext &f_context);
void cmdUnShake(akashi::CommandContext &f_context);
void cmdMedieval(akashi::CommandContext &f_context);
void cmdUnmedieval(akashi::CommandContext &f_context);
void cmdMedievalMode(akashi::CommandContext &f_context);
void cmdMutePM(akashi::CommandContext &f_context);
void cmdToggleAdverts(akashi::CommandContext &f_context);
void cmdAfk(akashi::CommandContext &f_context);
void cmdCharCurse(akashi::CommandContext &f_context);
void cmdUnCharCurse(akashi::CommandContext &f_context);
void cmdCharSelect(akashi::CommandContext &f_context);
void cmdForceCharSelect(akashi::CommandContext &f_context);
void cmdA(akashi::CommandContext &f_context);
void cmdS(akashi::CommandContext &f_context);
void cmdFirstPerson(akashi::CommandContext &f_context);

void registerMessagingCommands(akashi::CommandRegistry &f_registry);

} // namespace akashi::commands
