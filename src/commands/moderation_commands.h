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
void cmdLiftSanction(akashi::CommandContext &f_context);
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
void cmdWhy(akashi::CommandContext &f_context);
void cmdDumpGrants(akashi::CommandContext &f_context);

void registerModerationCommands(akashi::CommandRegistry &f_registry);

// Places a sanction through the server's one door: stored under the
// target's IPID and HWID so it survives reconnects and restarts, flagged
// on every window the person owns, with an optional trailing time after
// which it lifts itself - without one it holds until lifted by hand.
// Returns false when a time was given but could not be read; an error
// was already replied. f_data is the sanction's payload, if it has one.
bool applySanction(akashi::CommandContext &f_context, akashi::TargetPlayer &f_target, const QString &f_sanction_id, const QString &f_data = QString());

// Lifts a sanction by hand: the stored row, the pending lift and the
// session flags of everyone it covered all clear.
void liftSanction(akashi::CommandContext &f_context, akashi::TargetPlayer &f_target, const QString &f_sanction_id);

} // namespace akashi::commands
