#pragma once

namespace akashi {
class CommandRegistry;
}

namespace akashi::commands {

void registerAuthenticationCommands(akashi::CommandRegistry &f_registry);

} // namespace akashi::commands

