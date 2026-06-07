#pragma once

#include "akashi_core_export.h"

class Server;

namespace akashi {

class RuleRegistry;

// Registers the rule actions that ship with the core server. Before-actions
// gate events; after-actions populate the client or react to the world.
AKASHI_CORE_EXPORT void registerCoreRuleActions(Server *f_server, RuleRegistry *f_registry);

} // namespace akashi
