#pragma once

namespace akashi {
class CommandContext;
class CommandRegistry;
}

namespace akashi::commands {

// The command handlers this file exports to the registry.
void cmdGetArea(akashi::CommandContext &f_context);
void cmdGetAreas(akashi::CommandContext &f_context);
void cmdArea(akashi::CommandContext &f_context);
void cmdCm(akashi::CommandContext &f_context);
void cmdUncmOwn(akashi::CommandContext &f_context);
void cmdUncmOther(akashi::CommandContext &f_context);
void cmdInvite(akashi::CommandContext &f_context);
void cmdUninvite(akashi::CommandContext &f_context);
void cmdAreaLock(akashi::CommandContext &f_context);
void cmdAreaSpectate(akashi::CommandContext &f_context);
void cmdAreaUnlock(akashi::CommandContext &f_context);
void cmdAreaKick(akashi::CommandContext &f_context);
void cmdBackground(akashi::CommandContext &f_context);
void cmdSide(akashi::CommandContext &f_context);
void cmdLockBackground(akashi::CommandContext &f_context);
void cmdUnlockBackground(akashi::CommandContext &f_context);
void cmdStatus(akashi::CommandContext &f_context);
void cmdJudgeLog(akashi::CommandContext &f_context);
void cmdIgnoreBgList(akashi::CommandContext &f_context);
void cmdAreaMessage(akashi::CommandContext &f_context);
void cmdToggleMessage(akashi::CommandContext &f_context);
void cmdClearMessage(akashi::CommandContext &f_context);
void cmdToggleWtce(akashi::CommandContext &f_context);
void cmdToggleShouts(akashi::CommandContext &f_context);
void cmdFloors(akashi::CommandContext &f_context);
void cmdFloor(akashi::CommandContext &f_context);
void cmdCreateArea(akashi::CommandContext &f_context);
void cmdCreateFloor(akashi::CommandContext &f_context);
void cmdRenameArea(akashi::CommandContext &f_context);
void cmdRenameFloor(akashi::CommandContext &f_context);
void cmdRemoveArea(akashi::CommandContext &f_context);
void cmdRemoveFloor(akashi::CommandContext &f_context);
void cmdSaveWorld(akashi::CommandContext &f_context);
void cmdLoadWorld(akashi::CommandContext &f_context);
void cmdSaveFloor(akashi::CommandContext &f_context);
void cmdLoadFloor(akashi::CommandContext &f_context);
void cmdWebfiles(akashi::CommandContext &f_context);

void registerAreaCommands(akashi::CommandRegistry &f_registry);

} // namespace akashi::commands
