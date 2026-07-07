#include "commands/plugin_commands.h"

#include "akashi/permissions.h"
#include "akashi/service_registry.h"
#include "core/command_context.h"
#include "core/command_registry.h"
#include "core/plugin_manager.h"

namespace akashi::commands {

static QString stateLabel(PluginInfo::State f_state)
{
    switch (f_state) {
    case PluginInfo::State::Discovered:
        return QStringLiteral("Discovered");
    case PluginInfo::State::Loaded:
        return QStringLiteral("Loaded");
    case PluginInfo::State::Initialized:
        return QStringLiteral("Initialized");
    case PluginInfo::State::Started:
        return QStringLiteral("Started");
    case PluginInfo::State::Failed:
        return QStringLiteral("Failed");
    }
    return QStringLiteral("Unknown");
}

static PluginManager *resolveManager(CommandContext &f_ctx)
{
    auto l_mgr = f_ctx.services()->resolve<PluginManager>(QStringLiteral("akashi.plugins"));
    if (!l_mgr) {
        f_ctx.reply(QStringLiteral("Plugin system is not available."));
        return nullptr;
    }
    return l_mgr.get();
}

static void handlePluginList(CommandContext &f_ctx)
{
    PluginManager *l_mgr = resolveManager(f_ctx);
    if (!l_mgr)
        return;

    const auto l_plugins = l_mgr->plugins();
    if (l_plugins.isEmpty()) {
        f_ctx.reply(QStringLiteral("No plugins discovered."));
        return;
    }

    QStringList l_lines;
    for (const PluginInfo &l_info : l_plugins) {
        QString l_line = l_info.id + QStringLiteral(" v") + l_info.version.toString() + QStringLiteral(" [") + stateLabel(l_info.state) + QStringLiteral("]");
        if (l_info.state == PluginInfo::State::Started)
            l_line += QStringLiteral(" booted ") + QString::number(l_info.boot_ms) + QStringLiteral(" ms");
        if (!l_info.dependencies.isEmpty())
            l_line += QStringLiteral(" deps: ") + l_info.dependencies.join(QStringLiteral(", "));
        if (!l_info.services.isEmpty())
            l_line += QStringLiteral(" services: ") + l_info.services.join(QStringLiteral(", "));
        l_lines.append(l_line);
    }
    f_ctx.reply(l_lines.join(QStringLiteral("\n")));
}

static void handlePluginLoad(CommandContext &f_ctx)
{
    PluginManager *l_mgr = resolveManager(f_ctx);
    if (!l_mgr)
        return;

    const QString l_id = f_ctx.argument(1);
    if (l_mgr->loadPlugin(l_id)) {
        const auto l_info = l_mgr->pluginInfo(l_id);
        f_ctx.reply(QStringLiteral("Plugin loaded: ") + l_id + (l_info ? QStringLiteral(" v") + l_info->version.toString() : QString()));
    }
    else {
        f_ctx.reply(QStringLiteral("Failed to load plugin: ") + l_id);
    }
}

static void handlePluginUnload(CommandContext &f_ctx)
{
    PluginManager *l_mgr = resolveManager(f_ctx);
    if (!l_mgr)
        return;

    const QString l_id = f_ctx.argument(1);
    bool l_cascade = f_ctx.argc() > 2 && f_ctx.argument(2) == QStringLiteral("--cascade");

    if (l_mgr->unloadPlugin(l_id, l_cascade))
        f_ctx.reply(QStringLiteral("Plugin unloaded: ") + l_id);
    else
        f_ctx.reply(QStringLiteral("Failed to unload plugin: ") + l_id + QStringLiteral(". Other plugins may depend on it (use --cascade)."));
}

static void handlePluginReload(CommandContext &f_ctx)
{
    PluginManager *l_mgr = resolveManager(f_ctx);
    if (!l_mgr)
        return;

    const QString l_id = f_ctx.argument(1);
    if (l_mgr->reloadPlugin(l_id)) {
        const auto l_info = l_mgr->pluginInfo(l_id);
        f_ctx.reply(QStringLiteral("Plugin reloaded: ") + l_id + (l_info ? QStringLiteral(" v") + l_info->version.toString() : QString()));
    }
    else {
        f_ctx.reply(QStringLiteral("Failed to reload plugin: ") + l_id);
    }
}

static void handlePlugin(CommandContext &f_ctx)
{
    if (f_ctx.argc() < 1) {
        f_ctx.reply(QStringLiteral("Usage: /plugin <list|load|unload|reload> [id] [--cascade]"));
        return;
    }

    const QString l_sub = f_ctx.argument(0);
    if (l_sub == QStringLiteral("list"))
        handlePluginList(f_ctx);
    else if (l_sub == QStringLiteral("load"))
        handlePluginLoad(f_ctx);
    else if (l_sub == QStringLiteral("unload"))
        handlePluginUnload(f_ctx);
    else if (l_sub == QStringLiteral("reload"))
        handlePluginReload(f_ctx);
    else
        f_ctx.reply(QStringLiteral("Unknown subcommand: ") + l_sub + QStringLiteral(". Use list, load, unload, or reload."));
}

void registerPluginCommands(CommandRegistry &f_registry)
{
    CommandSpec l_spec;
    l_spec.name = QStringLiteral("plugin");
    l_spec.permissions = {permission::super};
    l_spec.min_args = 1;
    l_spec.usage = QStringLiteral("/plugin <list|load|unload|reload> [id] [--cascade]");
    l_spec.description = QStringLiteral("Manage server plugins.");

    f_registry.registerCommand(l_spec, handlePlugin, QStringLiteral("core"));
}

} // namespace akashi::commands
