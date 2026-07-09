#pragma once

namespace akashi {
class CommandRegistry;
class CommandContext;
}

namespace akashi::commands {

// The command handlers this file exports to the registry.
void cmdPlugin(akashi::CommandContext &f_context);

void registerPluginCommands(akashi::CommandRegistry &f_registry);

} // namespace akashi::commands
