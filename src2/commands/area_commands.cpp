#include "commands/area_commands.h"

#include "area_data.h"
#include "aoclient.h"
#include "core/command_context.h"
#include "core/command_registry.h"
#include "server.h"

namespace akashi::commands {

static QStringList buildAreaList(CommandContext &f_context, int f_area_index)
{
    Server *l_server = f_context.server();
    AreaData *l_area = l_server->areaById(f_area_index);
    QStringList l_entries;

    l_entries.append("=== " + l_server->areaName(f_area_index) + " ===");
    switch (l_area->lockStatus()) {
    case AreaData::LockStatus::LOCKED:
        l_entries.append("[LOCKED]");
        break;
    case AreaData::LockStatus::SPECTATABLE:
        l_entries.append("[SPECTATABLE]");
        break;
    case AreaData::LockStatus::FREE:
    default:
        break;
    }
    l_entries.append("[" + QString::number(l_area->playerCount()) + " users][" + l_area->statusLine() + "]");

    const QVector<AOClient *> l_clients = l_server->clients();
    for (AOClient *l_client : l_clients) {
        if (l_client->areaId() == f_area_index && l_client->isJoined()) {
            QString l_char_entry = "[" + QString::number(l_client->clientId()) + "] " + l_client->character();
            if (l_client->character().isEmpty()) {
                l_char_entry += "Spectator";
            }
            if (!l_client->characterName().isEmpty()) {
                l_char_entry += " (" + l_client->characterName() + ")";
            }
            if (l_area->owners().contains(l_client->clientId())) {
                l_char_entry.insert(0, "[CM] ");
            }
            if (f_context.isAuthenticated()) {
                l_char_entry += " (" + l_client->ipid() + "): " + l_client->name();
            }
            l_entries.append(l_char_entry);
        }
    }
    return l_entries;
}

static void handleGetArea(CommandContext &f_context)
{
    QStringList l_entries = buildAreaList(f_context, f_context.areaId());
    f_context.reply(l_entries.join("\n"));
}

static void handleGetAreas(CommandContext &f_context)
{
    Server *l_server = f_context.server();
    QStringList l_entries;
    l_entries.append("\n== Currently Online: " + QString::number(l_server->playerCount()) + " ==");
    for (int i = 0; i < l_server->areaCount(); i++) {
        if (l_server->areaById(i)->playerCount() > 0) {
            l_entries.append(buildAreaList(f_context, i));
        }
    }
    f_context.reply(l_entries.join("\n"));
}

static void handleArea(CommandContext &f_context)
{
    if (auto l_new_area = f_context.argumentAsInt(0)) {
        Server *l_server = f_context.server();
        if (*l_new_area < 0 || *l_new_area >= l_server->areaCount()) {
            f_context.reply("That does not look like a valid area ID.");
            return;
        }
        AOClient *l_client = l_server->clientById(f_context.clientId());
        if (l_client) {
            l_client->changeArea(*l_new_area);
        }
    }
    else {
        f_context.reply("That does not look like a valid area ID.");
    }
}

void registerAreaCommands(CommandRegistry &f_registry)
{
    f_registry.registerCommand(
        {QStringLiteral("getarea"), {}, {}, 0,
         QStringLiteral("/getarea"),
         QStringLiteral("Lists all clients in the area you are in.")},
        handleGetArea, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("getareas"), {}, {}, 0,
         QStringLiteral("/getareas"),
         QStringLiteral("Lists all clients in all areas.")},
        handleGetAreas, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("area"), {}, {}, 1,
         QStringLiteral("/area <id>"),
         QStringLiteral("Moves you to the area with the given ID.")},
        handleArea, QStringLiteral("core"));
}

} // namespace akashi::commands
