#pragma once

class ServerSettings;

class QString;

namespace akashi {
class CommandRegistry;
class CommandContext;
}

namespace akashi::commands {

// The command handlers this file exports to the registry.
void cmdLogin(akashi::CommandContext &f_context);
void cmdAddUser(akashi::CommandContext &f_context);
void cmdRemoveUser(akashi::CommandContext &f_context);
void cmdListPermsSelf(akashi::CommandContext &f_context);
void cmdListPermsUser(akashi::CommandContext &f_context);
void cmdSetPerms(akashi::CommandContext &f_context);
void cmdRemovePerms(akashi::CommandContext &f_context);
void cmdListUsers(akashi::CommandContext &f_context);
void cmdLogout(akashi::CommandContext &f_context);
void cmdChangePassSelf(akashi::CommandContext &f_context);
void cmdChangePassUser(akashi::CommandContext &f_context);

void registerAuthenticationCommands(akashi::CommandRegistry &f_registry);

// True when the password satisfies the owner-configured requirements.
bool passwordMeetsRequirements(ServerSettings *f_settings, const QString &f_username, const QString &f_password);

} // namespace akashi::commands
