#pragma once

namespace akashi {
class CommandRegistry;
}

namespace akashi::commands {

void registerPluginCommands(akashi::CommandRegistry &f_registry);

} // namespace akashi::commands

