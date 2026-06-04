#include "commands/area_commands.h"

#include "akashi/config_store.h"
#include "akashi/permissions.h"
#include "aoclient.h"
#include "area_data.h"
#include "core/command_context.h"
#include "core/command_registry.h"
#include "proto/packet.h"
#include "server.h"

#include <QRegularExpression>

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

static void handleCM(CommandContext &f_context)
{
    QString l_sender_name = f_context.name();
    AreaData *l_area = f_context.server()->areaById(f_context.areaId());
    if (l_area->isProtected() && !f_context.canPerform(akashi::permission::super)) {
        f_context.reply("This area is protected, you may not become CM in this area.");
        return;
    }
    else if (l_area->owners().isEmpty()) {
        l_area->addOwner(f_context.clientId());
        f_context.replyToArea(l_sender_name + " is now CM in this area.");
    }
    else if (!l_area->owners().contains(f_context.clientId())) {
        f_context.reply("You cannot become a CM in this area when someone else is. You must be CM'ed by an existing one.");
    }
    else if (f_context.argc() == 1) {
        bool ok;
        AOClient *l_owner_candidate = f_context.server()->clientById(f_context.argument(0).toInt(&ok));
        if (!ok) {
            f_context.reply("That doesn't look like a valid ID.");
            return;
        }
        if (l_owner_candidate == nullptr) {
            f_context.reply("Unable to find client with ID " + f_context.argument(0) + ".");
            return;
        }
        if (l_area->owners().contains(l_owner_candidate->clientId())) {
            f_context.reply("User is already a CM in this area.");
            return;
        }
        l_area->addOwner(l_owner_candidate->clientId());
        f_context.replyToArea(l_owner_candidate->name() + " is now CM in this area.");
    }
    else {
        f_context.reply("You are already a CM in this area.");
    }
}

static void handleUnCM(CommandContext &f_context)
{
    AreaData *l_area = f_context.server()->areaById(f_context.areaId());
    int l_uid;

    if (l_area->owners().isEmpty()) {
        f_context.reply("There are no CMs in this area.");
        return;
    }
    else if (f_context.argc() == 0) {
        l_uid = f_context.clientId();
        f_context.reply("You are no longer CM in this area.");
    }
    else if (f_context.canPerform(akashi::permission::remove_gamemaster) && f_context.argc() >= 1) {
        if (f_context.argument(0) == "all") {
            QList<int> owners = l_area->owners();
            for (int uid : owners) {
                if (uid != f_context.clientId()) {
                    l_area->removeOwner(uid);
                    AOClient *l_target = f_context.server()->clientById(uid);
                    if (l_target != nullptr) {
                        l_target->sendServerMessage("You have been unCMed.");
                    }
                }
            }
            f_context.reply("All CMs except yourself have been unCMed.");
            return;
        }

        bool conv_ok = false;
        l_uid = f_context.argument(0).toInt(&conv_ok);
        if (!conv_ok) {
            f_context.reply("Invalid user ID.");
            return;
        }
        if (!l_area->owners().contains(l_uid)) {
            f_context.reply("That user is not CMed.");
            return;
        }
        AOClient *l_target = f_context.server()->clientById(l_uid);
        if (l_target == nullptr) {
            f_context.reply("No client with that ID found.");
            return;
        }
        f_context.reply(l_target->name() + " was successfully unCMed.");
        l_target->sendServerMessage("You have been unCMed by a moderator.");
    }
    else {
        f_context.reply("You do not have permission to unCM others. Only yourself.");
        return;
    }

    l_area->removeOwner(l_uid);
}

static void handleInvite(CommandContext &f_context)
{
    AreaData *l_area = f_context.server()->areaById(f_context.areaId());
    bool ok;
    int l_invited_id = f_context.argument(0).toInt(&ok);
    if (!ok) {
        f_context.reply("That does not look like a valid ID.");
        return;
    }

    AOClient *target_client = f_context.server()->clientById(l_invited_id);
    if (target_client == nullptr) {
        f_context.reply("No client with that ID found.");
        return;
    }
    else if (!l_area->invite(l_invited_id)) {
        f_context.reply("That ID is already on the invite list.");
        return;
    }
    f_context.reply("You invited ID " + f_context.argument(0));
    target_client->sendServerMessage("You were invited and given access to " + l_area->name());
}

static void handleUnInvite(CommandContext &f_context)
{
    AreaData *l_area = f_context.server()->areaById(f_context.areaId());
    bool ok;
    int l_uninvited_id = f_context.argument(0).toInt(&ok);
    if (!ok) {
        f_context.reply("That does not look like a valid ID.");
        return;
    }

    AOClient *target_client = f_context.server()->clientById(l_uninvited_id);
    if (target_client == nullptr) {
        f_context.reply("No client with that ID found.");
        return;
    }
    else if (l_area->owners().contains(l_uninvited_id)) {
        f_context.reply("You cannot uninvite a CM!");
        return;
    }
    else if (!l_area->uninvite(l_uninvited_id)) {
        f_context.reply("That ID is not on the invite list.");
        return;
    }
    f_context.reply("You uninvited ID " + f_context.argument(0));
    target_client->sendServerMessage("You were uninvited from " + l_area->name());
}

static void handleLock(CommandContext &f_context)
{
    AreaData *l_area = f_context.server()->areaById(f_context.areaId());
    if (l_area->lockStatus() == AreaData::LockStatus::LOCKED) {
        f_context.reply("This area is already locked.");
        return;
    }
    f_context.replyToArea("This area is now locked.");
    l_area->lock();
    const QVector<AOClient *> l_clients = f_context.server()->clients();
    for (AOClient *l_client : l_clients) {
        if (l_client->areaId() == f_context.areaId() && l_client->isJoined()) {
            l_area->invite(l_client->clientId());
        }
    }
}

static void handleSpectatable(CommandContext &f_context)
{
    AreaData *l_area = f_context.server()->areaById(f_context.areaId());
    if (l_area->lockStatus() == AreaData::LockStatus::SPECTATABLE) {
        f_context.reply("This area is already in spectate mode.");
        return;
    }
    f_context.replyToArea("This area is now spectatable.");
    l_area->spectatable();
    const QVector<AOClient *> l_clients = f_context.server()->clients();
    for (AOClient *l_client : l_clients) {
        if (l_client->areaId() == f_context.areaId() && l_client->isJoined()) {
            l_area->invite(l_client->clientId());
        }
    }
}

static void handleUnLock(CommandContext &f_context)
{
    AreaData *l_area = f_context.server()->areaById(f_context.areaId());
    if (l_area->lockStatus() == AreaData::LockStatus::FREE) {
        f_context.reply("This area is not locked.");
        return;
    }
    f_context.replyToArea("This area is now unlocked.");
    l_area->unlock();
}

static void handleAreaKick(CommandContext &f_context)
{
    AreaData *l_area = f_context.server()->areaById(f_context.areaId());

    int target_area_id = 0;

    if (f_context.argc() >= 2) {
        if (!f_context.canPerform(akashi::permission::kick)) {
            f_context.reply("You do not have permission to kick to specific areas. Just the first area as CM. (/areakick [ID]).");
            return;
        }

        bool ok;
        target_area_id = f_context.argument(1).toInt(&ok);
        if (!ok || target_area_id < 0 || target_area_id >= f_context.server()->areaCount()) {
            f_context.reply("That does not look like a valid area ID.");
            return;
        }
    }

    AreaData *target_area = f_context.server()->areaById(target_area_id);

    if (f_context.argument(0) == "all") {
        const QVector<AOClient *> l_clients = f_context.server()->clients();
        for (AOClient *l_client : l_clients) {
            if (l_client->areaId() == f_context.areaId() && l_client->clientId() != f_context.clientId()) {
                if (!f_context.server()->areaById(f_context.areaId())->owners().contains(l_client->clientId())) {
                    l_client->changeArea(target_area_id);
                    l_area->uninvite(l_client->clientId());
                    l_client->sendServerMessage("You have been kicked to area " + target_area->displayName() + ".");
                }
            }
        }
        f_context.reply("All clients kicked to area " + target_area->displayName() + ".");
        return;
    }

    bool ok;
    int l_idx = f_context.argument(0).toInt(&ok);
    if (!ok) {
        f_context.reply("That does not look like a valid ID.");
        return;
    }
    if (f_context.server()->areaById(f_context.areaId())->owners().contains(l_idx)) {
        f_context.reply("You cannot kick another CM!");
        return;
    }
    AOClient *l_client_to_kick = f_context.server()->clientById(l_idx);
    if (l_client_to_kick == nullptr) {
        f_context.reply("No client with that ID found.");
        return;
    }
    else if (l_client_to_kick->areaId() != f_context.areaId()) {
        f_context.reply("That client is not in this area.");
        return;
    }
    l_client_to_kick->changeArea(target_area_id);
    l_area->uninvite(l_client_to_kick->clientId());
    l_client_to_kick->sendServerMessage("You have been kicked to area " + target_area->displayName() + ".");
    f_context.reply("Client " + f_context.argument(0) + " kicked to area " + target_area->displayName() + ".");
}

static void handleSetBackground(CommandContext &f_context)
{
    QString l_background = f_context.arguments().join(" ");
    AreaData *l_area = f_context.server()->areaById(f_context.areaId());
    if (f_context.isAuthenticated() || !l_area->isBgLocked()) {
        if (l_area->lockStatus() == AreaData::LockStatus::SPECTATABLE && !l_area->invited().contains(f_context.clientId()) && !f_context.canPerform(akashi::permission::bypass_locks)) {
            f_context.reply("Spectators are blocked from changing the background.");
            return;
        }
        if (f_context.server()->backgrounds().contains(l_background, Qt::CaseInsensitive) || l_area->ignoreBgList() == true) {
            l_area->setBackground(l_background);
            f_context.server()->broadcast(akashi::Packet("BN", {l_background, l_area->side()}), f_context.areaId());
            QString ambience_name = f_context.server()->configStore()->settings("ambience")->value(l_background + "/ambience").toString();
            if (ambience_name != "") {
                f_context.server()->broadcast(akashi::Packet("MC", {ambience_name, "-1", f_context.characterName(), "1", "1"}), f_context.areaId());
            }
            else {
                f_context.server()->broadcast(akashi::Packet("MC", {"~stop.mp3", "-1", f_context.characterName(), "1", "1"}), f_context.areaId());
            }
            f_context.replyToArea(f_context.character() + " changed the background to " + l_background);
        }
        else {
            f_context.reply("Invalid background name.");
        }
    }
    else {
        f_context.reply("This area's background is locked.");
    }
}

static void handleSetSide(CommandContext &f_context)
{
    AreaData *l_area = f_context.server()->areaById(f_context.areaId());
    if (l_area->isBgLocked()) {
        f_context.reply("This area's background is locked.");
        return;
    }

    QString l_side = f_context.arguments().join(" ");
    l_area->setSide(l_side);
    f_context.server()->broadcast(akashi::Packet("BN", {l_area->background(), l_side}), f_context.areaId());
    if (l_side.isEmpty()) {
        f_context.replyToArea(f_context.character() + " unlocked the background side");
    }
    else {
        f_context.replyToArea(f_context.character() + " locked the background side to " + l_side);
    }
}

static void handleBgLock(CommandContext &f_context)
{
    AreaData *l_area = f_context.server()->areaById(f_context.areaId());

    if (l_area->isBgLocked() == false) {
        l_area->toggleBgLock();
    };

    f_context.server()->broadcast(akashi::Packet("CT", {f_context.server()->serverNickname(), f_context.character() + " locked the background.", "1"}), f_context.areaId());
}

static void handleBgUnlock(CommandContext &f_context)
{
    AreaData *l_area = f_context.server()->areaById(f_context.areaId());

    if (l_area->isBgLocked() == true) {
        l_area->toggleBgLock();
    };

    f_context.server()->broadcast(akashi::Packet("CT", {f_context.server()->serverNickname(), f_context.character() + " unlocked the background.", "1"}), f_context.areaId());
}

static void handleStatus(CommandContext &f_context)
{
    AreaData *l_area = f_context.server()->areaById(f_context.areaId());
    QString l_arg = f_context.argument(0).toLower();

    if (l_area->changeStatus(l_arg)) {
        f_context.server()->broadcast(akashi::Packet("CT", {f_context.server()->serverNickname(), f_context.character() + " changed status to " + l_arg.toUpper(), "1"}), f_context.areaId());
    }
    else {
        const QStringList keys = AreaData::map_statuses.keys();
        f_context.reply("That does not look like a valid status. Valid statuses are " + keys.join(", "));
    }
}

static void handleJudgeLog(CommandContext &f_context)
{
    AreaData *l_area = f_context.server()->areaById(f_context.areaId());
    if (l_area->judgelog().isEmpty()) {
        f_context.reply("There have been no judge actions in this area.");
        return;
    }
    QString l_message = l_area->judgelog().join("\n");
    if (f_context.canPerform(akashi::permission::kick) || f_context.canPerform(akashi::permission::ban)) {
        f_context.reply(l_message);
    }
    else {
        QString filteredmessage = l_message.remove(QRegularExpression("[(].*[)]"));
        f_context.reply(filteredmessage);
    }
}

static void handleIgnoreBgList(CommandContext &f_context)
{
    AreaData *l_area = f_context.server()->areaById(f_context.areaId());
    l_area->toggleIgnoreBgList();
    QString l_state = l_area->ignoreBgList() ? "ignored." : "enforced.";
    f_context.reply("BG list in this area is now " + l_state);
}

static void handleAreaMessage(CommandContext &f_context)
{
    AreaData *l_area = f_context.server()->areaById(f_context.areaId());
    if (f_context.argc() == 0) {
        f_context.reply(l_area->areaMessage());
        return;
    }

    l_area->changeAreaMessage(f_context.arguments().join(" "));
    f_context.reply("Updated this area's message.");
}

static void handleToggleAreaMessageOnJoin(CommandContext &f_context)
{
    AreaData *l_area = f_context.server()->areaById(f_context.areaId());
    l_area->toggleAreaMessageJoin();
    QString l_state = l_area->sendAreaMessageOnJoin() ? "enabled." : "disabled.";
    f_context.reply("Sending message on area join is now " + l_state);
}

static void handleClearAreaMessage(CommandContext &f_context)
{
    AreaData *l_area = f_context.server()->areaById(f_context.areaId());
    l_area->clearAreaMessage();
    if (l_area->sendAreaMessageOnJoin()) {
        handleToggleAreaMessageOnJoin(f_context);
    }
}

static void handleToggleWtce(CommandContext &f_context)
{
    AreaData *l_area = f_context.server()->areaById(f_context.areaId());
    l_area->toggleWtceAllowed();
    QString l_state = l_area->isWtceAllowed() ? "enabled." : "disabled.";
    f_context.reply("Using testimony animations is now " + l_state);
}

static void handleToggleShouts(CommandContext &f_context)
{
    AreaData *l_area = f_context.server()->areaById(f_context.areaId());
    l_area->toggleShoutAllowed();
    QString l_state = l_area->isShoutAllowed() ? "enabled." : "disabled.";
    f_context.reply("Using shouts is now " + l_state);
}

static void handleFloor(CommandContext &f_context);

static void handleFloors(CommandContext &f_context)
{
    if (!f_context.arguments().isEmpty()) {
        handleFloor(f_context);
        return;
    }
    Server *l_server = f_context.server();
    QStringList l_lines;
    l_lines.append("== Floors ==");
    for (int i = 0; i < l_server->floorCount(); i++) {
        const akashi::Floor *l_floor = l_server->floorById(i);
        QStringList l_area_names;
        for (int l_aid : l_floor->area_ids) {
            l_area_names.append(l_server->areaName(l_aid));
        }
        l_lines.append("[" + QString::number(i) + "] " + l_floor->name + ": " + l_area_names.join(", "));
    }
    f_context.reply(l_lines.join("\n"));
}

static void handleFloor(CommandContext &f_context)
{
    Server *l_server = f_context.server();
    QString l_arg = f_context.argument(0);

    const akashi::Floor *l_floor = nullptr;
    bool l_ok = false;
    int l_id = l_arg.toInt(&l_ok);
    if (l_ok) {
        l_floor = l_server->floorById(l_id);
    }
    else {
        l_floor = l_server->floorByName(l_arg);
    }

    if (!l_floor) {
        f_context.reply("No floor found with that name or ID. Use /floors to list them.");
        return;
    }
    if (l_floor->area_ids.isEmpty()) {
        f_context.reply("That floor has no areas.");
        return;
    }

    AOClient *l_client = l_server->clientById(f_context.clientId());
    if (l_client) {
        l_client->changeArea(l_floor->area_ids.first());
    }
}

static void handleWebfiles(CommandContext &f_context)
{
    const QVector<AOClient *> l_clients = f_context.server()->clients();
    QStringList l_weblinks;
    for (AOClient *l_client : l_clients) {
        if (l_client->iniswap().isEmpty() || l_client->areaId() != f_context.areaId()) {
            continue;
        }

        if (l_client->character().toLower() != l_client->iniswap().toLower()) {
            l_weblinks.append("https://attorneyonline.github.io/webDownloader/index.html?char=" + l_client->iniswap());
        }
    }
    f_context.reply("Character files:\n" + l_weblinks.join("\n"));
}

void registerAreaCommands(CommandRegistry &f_registry)
{
    f_registry.registerCommand(
        {QStringLiteral("getarea"), {QStringLiteral("ga")}, {}, 0,
         QStringLiteral("/getarea"),
         QStringLiteral("Lists all clients in the area you are in.")},
        handleGetArea, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("getareas"), {QStringLiteral("gas")}, {}, 0,
         QStringLiteral("/getareas"),
         QStringLiteral("Lists all clients in all areas.")},
        handleGetAreas, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("area"), {}, {}, 1,
         QStringLiteral("/area <id>"),
         QStringLiteral("Moves you to the area with the given ID.")},
        handleArea, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("cm"), {}, {}, 0,
         QStringLiteral("/cm [id]"),
         QStringLiteral("Claims CM or adds another client as CM.")},
        handleCM, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("uncm"), {}, {QStringLiteral("gamemaster")}, 0,
         QStringLiteral("/uncm [id|all]"),
         QStringLiteral("Removes CM status from yourself or another client.")},
        handleUnCM, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("invite"), {}, {QStringLiteral("gamemaster")}, 1,
         QStringLiteral("/invite <id>"),
         QStringLiteral("Invites a client to the area.")},
        handleInvite, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("uninvite"), {}, {QStringLiteral("gamemaster")}, 1,
         QStringLiteral("/uninvite <id>"),
         QStringLiteral("Removes a client from the area invite list.")},
        handleUnInvite, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("area_lock"), {QStringLiteral("lock_area"), QStringLiteral("lock")}, {QStringLiteral("gamemaster")}, 0,
         QStringLiteral("/area_lock"),
         QStringLiteral("Locks the area so only invited clients may enter.")},
        handleLock, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("area_spectate"), {QStringLiteral("spectatable")}, {QStringLiteral("gamemaster")}, 0,
         QStringLiteral("/area_spectate"),
         QStringLiteral("Sets the area to spectate-only mode.")},
        handleSpectatable, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("area_unlock"), {QStringLiteral("unlock_area"), QStringLiteral("unlock")}, {QStringLiteral("gamemaster")}, 0,
         QStringLiteral("/area_unlock"),
         QStringLiteral("Unlocks the area.")},
        handleUnLock, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("area_kick"), {QStringLiteral("kick_area"), QStringLiteral("areakick")}, {QStringLiteral("gamemaster")}, 1,
         QStringLiteral("/area_kick <id|all> [area]"),
         QStringLiteral("Kicks a client or all non-CMs from the area.")},
        handleAreaKick, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("background"), {QStringLiteral("bg")}, {}, 1,
         QStringLiteral("/background <name>"),
         QStringLiteral("Changes the background of the area.")},
        handleSetBackground, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("side"), {}, {QStringLiteral("gamemaster")}, 0,
         QStringLiteral("/side [name]"),
         QStringLiteral("Locks or unlocks the background side.")},
        handleSetSide, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("lock_background"), {QStringLiteral("lock_bg"), QStringLiteral("lockbg"), QStringLiteral("bglock")}, {QStringLiteral("gamemaster")}, 0,
         QStringLiteral("/lock_background"),
         QStringLiteral("Locks the background in the area.")},
        handleBgLock, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("unlock_background"), {QStringLiteral("unlock_bg"), QStringLiteral("unlockbg"), QStringLiteral("bgunlock")}, {QStringLiteral("gamemaster")}, 0,
         QStringLiteral("/unlock_background"),
         QStringLiteral("Unlocks the background in the area.")},
        handleBgUnlock, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("status"), {}, {}, 1,
         QStringLiteral("/status <status>"),
         QStringLiteral("Changes the status of the area.")},
        handleStatus, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("judgelog"), {}, {QStringLiteral("gamemaster")}, 0,
         QStringLiteral("/judgelog"),
         QStringLiteral("Displays the judge log for the area.")},
        handleJudgeLog, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("ignore_bglist"), {QStringLiteral("ignorebglist")}, {QStringLiteral("ignore_background_list")}, 0,
         QStringLiteral("/ignore_bglist"),
         QStringLiteral("Toggles whether the background list is enforced.")},
        handleIgnoreBgList, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("areamessage"), {}, {QStringLiteral("gamemaster")}, 0,
         QStringLiteral("/areamessage [message]"),
         QStringLiteral("Views or sets the area message.")},
        handleAreaMessage, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("togglemessage"), {}, {QStringLiteral("gamemaster")}, 0,
         QStringLiteral("/togglemessage"),
         QStringLiteral("Toggles sending the area message on join.")},
        handleToggleAreaMessageOnJoin, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("clearmessage"), {}, {QStringLiteral("gamemaster")}, 0,
         QStringLiteral("/clearmessage"),
         QStringLiteral("Clears the area message.")},
        handleClearAreaMessage, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("toggle_wtce"), {QStringLiteral("togglewtce")}, {QStringLiteral("gamemaster")}, 0,
         QStringLiteral("/toggle_wtce"),
         QStringLiteral("Toggles testimony animations in the area.")},
        handleToggleWtce, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("toggle_shouts"), {QStringLiteral("toggleshouts")}, {QStringLiteral("gamemaster")}, 0,
         QStringLiteral("/toggle_shouts"),
         QStringLiteral("Toggles shouts in the area.")},
        handleToggleShouts, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("floors"), {}, {}, 0,
         QStringLiteral("/floors"),
         QStringLiteral("Lists all floors and their areas.")},
        handleFloors, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("floor"), {}, {}, 1,
         QStringLiteral("/floor <name|id>"),
         QStringLiteral("Moves you to the first area on the given floor.")},
        handleFloor, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("webfiles"), {}, {}, 0,
         QStringLiteral("/webfiles"),
         QStringLiteral("Lists download links for iniswapped characters.")},
        handleWebfiles, QStringLiteral("core"));

}

} // namespace akashi::commands
