#pragma once

namespace akashi {
class CommandRegistry;
class CommandContext;
}

namespace akashi::commands {

// The command handlers this file exports to the registry.
void cmdPlay(akashi::CommandContext &f_context);
void cmdPlayAmbience(akashi::CommandContext &f_context);
void cmdCurrentMusic(akashi::CommandContext &f_context);
void cmdBlockDj(akashi::CommandContext &f_context);
void cmdUnblockDj(akashi::CommandContext &f_context);
void cmdToggleMusic(akashi::CommandContext &f_context);
void cmdToggleJukebox(akashi::CommandContext &f_context);
void cmdAddMusic(akashi::CommandContext &f_context);
void cmdAddMusicCategory(akashi::CommandContext &f_context);
void cmdRemoveCustomMusic(akashi::CommandContext &f_context);
void cmdToggleCustomMusic(akashi::CommandContext &f_context);
void cmdClearCustomMusic(akashi::CommandContext &f_context);
void cmdJukeboxSkip(akashi::CommandContext &f_context);

void registerMusicCommands(akashi::CommandRegistry &f_registry);

} // namespace akashi::commands
