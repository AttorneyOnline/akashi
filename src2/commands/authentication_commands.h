#ifndef COMMANDS_AUTHENTICATION_COMMANDS_H
#define COMMANDS_AUTHENTICATION_COMMANDS_H

namespace akashi {
class CommandRegistry;
}

namespace akashi::commands {

void registerAuthenticationCommands(akashi::CommandRegistry &f_registry);

} // namespace akashi::commands

#endif // COMMANDS_AUTHENTICATION_COMMANDS_H
