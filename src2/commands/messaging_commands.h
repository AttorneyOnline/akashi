#ifndef COMMANDS_MESSAGING_COMMANDS_H
#define COMMANDS_MESSAGING_COMMANDS_H

namespace akashi {
class CommandRegistry;
}

namespace akashi::commands {

void registerMessagingCommands(akashi::CommandRegistry &f_registry);

} // namespace akashi::commands

#endif // COMMANDS_MESSAGING_COMMANDS_H
