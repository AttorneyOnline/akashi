#pragma once

#include <QString>

namespace akashi {
class CommandRegistry;
class CommandContext;
class TargetPlayer;
}

namespace akashi::commands {

// The command handlers this file exports to the registry.
void cmdBan(akashi::CommandContext &f_context);
void cmdKick(akashi::CommandContext &f_context);
void cmdMods(akashi::CommandContext &f_context);
void cmdCommands(akashi::CommandContext &f_context);
void cmdHelp(akashi::CommandContext &f_context);
void cmdMotd(akashi::CommandContext &f_context);
void cmdSetMotd(akashi::CommandContext &f_context);
void cmdBans(akashi::CommandContext &f_context);
void cmdUnban(akashi::CommandContext &f_context);
void cmdAbout(akashi::CommandContext &f_context);
void cmdMute(akashi::CommandContext &f_context);
void cmdUnmute(akashi::CommandContext &f_context);
void cmdOocMute(akashi::CommandContext &f_context);
void cmdOocUnmute(akashi::CommandContext &f_context);
void cmdBlockWtce(akashi::CommandContext &f_context);
void cmdUnblockWtce(akashi::CommandContext &f_context);
void cmdAllowBlankposting(akashi::CommandContext &f_context);
void cmdBanInfo(akashi::CommandContext &f_context);
void cmdReload(akashi::CommandContext &f_context);
void cmdForceNointPres(akashi::CommandContext &f_context);
void cmdAllowIniswap(akashi::CommandContext &f_context);
void cmdPermitSaving(akashi::CommandContext &f_context);
void cmdKickUid(akashi::CommandContext &f_context);
void cmdUpdateBan(akashi::CommandContext &f_context);
void cmdNotice(akashi::CommandContext &f_context);
void cmdNoticeG(akashi::CommandContext &f_context);
void cmdKickOther(akashi::CommandContext &f_context);

void registerModerationCommands(akashi::CommandRegistry &f_registry);

// Handles the optional trailing time of a sanction command. With one, the
// sanction is stored under the target's IPID and lifts itself - it
// survives reconnects and restarts. Without one, any stored lift is
// dropped so the sanction holds until lifted by hand. Returns false when
// a time was given but could not be read; an error was already replied.
bool applySanctionSchedule(akashi::CommandContext &f_context, akashi::TargetPlayer &f_target, const QString &f_sanction_id);

// Drops the stored row and pending lift when a sanction is lifted by hand.
void clearSanctionSchedule(akashi::CommandContext &f_context, akashi::TargetPlayer &f_target, const QString &f_sanction_id);

} // namespace akashi::commands
