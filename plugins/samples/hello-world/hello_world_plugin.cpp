#include "hello_world_plugin.h"

#include "akashi/service_registry.h"
#include "core/command_context.h"
#include "core/command_registry.h"
#include "core/command_spec.h"

#include <QDebug>

QString HelloWorldPlugin::id() const { return QStringLiteral("akashi.hello-world"); }
akashi::ServiceVersion HelloWorldPlugin::pluginVersion() const { return {1, 0, 0}; }

bool HelloWorldPlugin::load(akashi::ServiceRegistry &services)
{
    auto l_commands = services.resolve<akashi::CommandRegistry>(QStringLiteral("akashi.commands"));
    if (!l_commands)
        return false;

    akashi::CommandSpec l_spec;
    l_spec.name = QStringLiteral("hello");
    l_spec.gate = {QStringLiteral("none")};
    l_spec.usage = QStringLiteral("/hello");
    l_spec.description = QStringLiteral("Says hello from the plugin system.");

    l_commands->registerCommand(l_spec, &HelloWorldPlugin::cmdHello, id());

    return true;
}

void HelloWorldPlugin::cmdHello(akashi::CommandContext &f_context)
{
    f_context.reply(QStringLiteral("Hello from the plugin system!"));
}

void HelloWorldPlugin::shutdown(akashi::ServiceRegistry &services)
{
    Q_UNUSED(services);
}
