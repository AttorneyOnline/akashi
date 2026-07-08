#pragma once

namespace akashi {
class CommandRegistry;
}

namespace akashi::commands {

void registerMessagingCommands(akashi::CommandRegistry &f_registry);

} // namespace akashi::commands
