#pragma once

class ServerSettings;

class QString;

namespace akashi {
class CommandRegistry;
}

namespace akashi::commands {

void registerAuthenticationCommands(akashi::CommandRegistry &f_registry);

// True when the password satisfies the owner-configured requirements.
bool passwordMeetsRequirements(ServerSettings *f_settings, const QString &f_username, const QString &f_password);

} // namespace akashi::commands
