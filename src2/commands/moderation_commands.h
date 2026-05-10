#ifndef COMMANDS_MODERATION_COMMANDS_H
#define COMMANDS_MODERATION_COMMANDS_H

namespace akashi {
class CommandRegistry;
}

namespace akashi::commands {

void registerModerationCommands(akashi::CommandRegistry &f_registry);

} // namespace akashi::commands

#endif // COMMANDS_MODERATION_COMMANDS_H
