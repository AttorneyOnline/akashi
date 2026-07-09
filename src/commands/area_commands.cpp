#include "commands/area_commands.h"

#include "akashi/permissions.h"
#include "core/client_session.h"
#include "core/command_context.h"
#include "core/command_registry.h"
#include "core/server_context.h"
#include "proto/packet.h"
#include "world/area.h"

#include <QRegularExpression>

namespace akashi::commands {

static QStringList buildAreaList(CommandContext &f_context, int f_area_index)
{
    ServerContext *l_server = f_context.server();
    akashi::Area *l_area = l_server->areaById(f_area_index);
    QStringList l_entries;

    l_entries.append("=== " + l_server->areaName(f_area_index) + " ===");
    switch (l_area->lockState()) {
    case akashi::Area::LockState::Locked:
        l_entries.append("[LOCKED]");
        break;
    case akashi::Area::LockState::Spectatable:
        l_entries.append("[SPECTATABLE]");
        break;
    case akashi::Area::LockState::Free:
    default:
        break;
    }
    l_entries.append("[" + QString::number(l_area->playerCount()) + " users][" + l_area->status() + "]");

    const QVector<akashi::ClientSession *> l_clients = l_server->clients();
    for (akashi::ClientSession *l_client : l_clients) {
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

void cmdGetArea(CommandContext &f_context)
{
    QStringList l_entries = buildAreaList(f_context, f_context.areaId());
    f_context.reply(l_entries.join("\n"));
}

void cmdGetAreas(CommandContext &f_context)
{
    ServerContext *l_server = f_context.server();
    QStringList l_entries;
    l_entries.append("\n== Currently Online: " + QString::number(l_server->playerCount()) + " ==");
    for (int i = 0; i < l_server->areaCount(); i++) {
        if (l_server->areaById(i)->playerCount() > 0) {
            l_entries.append(buildAreaList(f_context, i));
        }
    }
    f_context.reply(l_entries.join("\n"));
}

void cmdArea(CommandContext &f_context)
{
    if (auto l_new_area = f_context.argumentAsInt(0)) {
        ServerContext *l_server = f_context.server();
        if (*l_new_area < 0 || *l_new_area >= l_server->areaCount()) {
            f_context.reply("That does not look like a valid area ID.");
            return;
        }
        akashi::ClientSession *l_client = l_server->clientById(f_context.clientId());
        if (l_client) {
            l_client->changeArea(*l_new_area);
        }
    }
    else {
        f_context.reply("That does not look like a valid area ID.");
    }
}

void cmdCm(CommandContext &f_context)
{
    QString l_sender_name = f_context.name();
    akashi::Area *l_area = f_context.server()->areaById(f_context.areaId());
    akashi::ClientSession *l_client = f_context.server()->clientById(f_context.clientId());
    if (!l_client) {
        return;
    }
    if (l_area->isProtected() && !f_context.canPerform(akashi::permission::super)) {
        f_context.reply("This area is protected, you may not become CM in this area.");
        return;
    }
    else if (l_area->owners().isEmpty()) {
        if (auto l_refusal = l_client->addAreaOwner(f_context.clientId())) {
            f_context.reply(*l_refusal);
            return;
        }
        f_context.replyToArea(l_sender_name + " is now CM in this area.");
    }
    else if (!l_area->owners().contains(f_context.clientId())) {
        f_context.reply("You cannot become a CM in this area when someone else is. You must be CM'ed by an existing one.");
    }
    else if (f_context.argc() == 1) {
        bool ok;
        akashi::ClientSession *l_owner_candidate = f_context.server()->clientById(f_context.argument(0).toInt(&ok));
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
        if (auto l_refusal = l_client->addAreaOwner(l_owner_candidate->clientId())) {
            f_context.reply(*l_refusal);
            return;
        }
        f_context.replyToArea(l_owner_candidate->name() + " is now CM in this area.");
    }
    else {
        f_context.reply("You are already a CM in this area.");
    }
}

void cmdUncmOwn(CommandContext &f_context)
{
    akashi::Area *l_area = f_context.server()->areaById(f_context.areaId());
    if (l_area->owners().isEmpty()) {
        f_context.reply("There are no CMs in this area.");
        return;
    }
    if (!l_area->owners().contains(f_context.clientId())) {
        f_context.reply("You are not a CM in this area.");
        return;
    }

    akashi::ClientSession *l_client = f_context.server()->clientById(f_context.clientId());
    if (!l_client) {
        return;
    }
    if (auto l_refusal = l_client->removeAreaOwner(f_context.clientId())) {
        f_context.reply(*l_refusal);
        return;
    }
    f_context.reply("You are no longer CM in this area.");
}

void cmdUncmOther(CommandContext &f_context)
{
    akashi::Area *l_area = f_context.server()->areaById(f_context.areaId());
    if (l_area->owners().isEmpty()) {
        f_context.reply("There are no CMs in this area.");
        return;
    }

    akashi::ClientSession *l_client = f_context.server()->clientById(f_context.clientId());
    if (!l_client) {
        return;
    }

    if (f_context.argument(0) == "all") {
        QList<int> owners = l_area->owners();
        for (int uid : owners) {
            if (uid != f_context.clientId()) {
                // A mid-loop block leaves the earlier removals applied.
                if (auto l_refusal = l_client->removeAreaOwner(uid)) {
                    f_context.reply(*l_refusal);
                    return;
                }
                akashi::ClientSession *l_target = f_context.server()->clientById(uid);
                if (l_target != nullptr) {
                    l_target->sendServerMessage("You have been unCMed.");
                }
            }
        }
        f_context.reply("All CMs except yourself have been unCMed.");
        return;
    }

    bool conv_ok = false;
    const int l_uid = f_context.argument(0).toInt(&conv_ok);
    if (!conv_ok) {
        f_context.reply("Invalid user ID.");
        return;
    }
    if (!l_area->owners().contains(l_uid)) {
        f_context.reply("That user is not CMed.");
        return;
    }
    akashi::ClientSession *l_target = f_context.server()->clientById(l_uid);
    if (l_target == nullptr) {
        f_context.reply("No client with that ID found.");
        return;
    }
    if (auto l_refusal = l_client->removeAreaOwner(l_uid)) {
        f_context.reply(*l_refusal);
        return;
    }
    f_context.reply(l_target->name() + " was successfully unCMed.");
    l_target->sendServerMessage("You have been unCMed by a moderator.");
}

void cmdInvite(CommandContext &f_context)
{
    akashi::Area *l_area = f_context.server()->areaById(f_context.areaId());
    bool ok;
    int l_invited_id = f_context.argument(0).toInt(&ok);
    if (!ok) {
        f_context.reply("That does not look like a valid ID.");
        return;
    }

    akashi::ClientSession *target_client = f_context.server()->clientById(l_invited_id);
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

void cmdUninvite(CommandContext &f_context)
{
    akashi::Area *l_area = f_context.server()->areaById(f_context.areaId());
    bool ok;
    int l_uninvited_id = f_context.argument(0).toInt(&ok);
    if (!ok) {
        f_context.reply("That does not look like a valid ID.");
        return;
    }

    akashi::ClientSession *target_client = f_context.server()->clientById(l_uninvited_id);
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

void cmdAreaLock(CommandContext &f_context)
{
    akashi::Area *l_area = f_context.server()->areaById(f_context.areaId());
    if (l_area->lockState() == akashi::Area::LockState::Locked) {
        f_context.reply("This area is already locked.");
        return;
    }
    akashi::ClientSession *l_client = f_context.server()->clientById(f_context.clientId());
    if (!l_client) {
        return;
    }
    if (auto l_refusal = l_client->setAreaLock(akashi::Area::LockState::Locked)) {
        f_context.reply(*l_refusal);
        return;
    }
    f_context.replyToArea("This area is now locked.");
}

void cmdAreaSpectate(CommandContext &f_context)
{
    akashi::Area *l_area = f_context.server()->areaById(f_context.areaId());
    if (l_area->lockState() == akashi::Area::LockState::Spectatable) {
        f_context.reply("This area is already in spectate mode.");
        return;
    }
    akashi::ClientSession *l_client = f_context.server()->clientById(f_context.clientId());
    if (!l_client) {
        return;
    }
    if (auto l_refusal = l_client->setAreaLock(akashi::Area::LockState::Spectatable)) {
        f_context.reply(*l_refusal);
        return;
    }
    f_context.replyToArea("This area is now spectatable.");
}

void cmdAreaUnlock(CommandContext &f_context)
{
    akashi::Area *l_area = f_context.server()->areaById(f_context.areaId());
    if (l_area->lockState() == akashi::Area::LockState::Free) {
        f_context.reply("This area is not locked.");
        return;
    }
    akashi::ClientSession *l_client = f_context.server()->clientById(f_context.clientId());
    if (!l_client) {
        return;
    }
    if (auto l_refusal = l_client->setAreaLock(akashi::Area::LockState::Free)) {
        f_context.reply(*l_refusal);
        return;
    }
    f_context.replyToArea("This area is now unlocked.");
}

void cmdAreaKick(CommandContext &f_context)
{
    akashi::Area *l_area = f_context.server()->areaById(f_context.areaId());

    int target_area_id = 0;

    if (f_context.argc() >= 2) {
        // Stays inline: this form needs gamemaster AND kick, and the variant
        // gate only expresses any-of.
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

    akashi::Area *target_area = f_context.server()->areaById(target_area_id);

    if (f_context.argument(0) == "all") {
        const QVector<akashi::ClientSession *> l_clients = f_context.server()->clients();
        for (akashi::ClientSession *l_client : l_clients) {
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
    akashi::ClientSession *l_client_to_kick = f_context.server()->clientById(l_idx);
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

void cmdBackground(CommandContext &f_context)
{
    akashi::ClientSession *l_client = f_context.server()->clientById(f_context.clientId());
    if (!l_client) {
        return;
    }
    if (!l_client->canActInArea()) {
        f_context.reply("Spectators are blocked from changing the background.");
        return;
    }
    // The verb owns the background rules, the validation, the BN broadcast,
    // the background's ambience and the area notice.
    if (auto l_refusal = l_client->changeBackground(f_context.arguments().join(" "))) {
        f_context.reply(*l_refusal);
    }
}

void cmdSide(CommandContext &f_context)
{
    akashi::ClientSession *l_client = f_context.server()->clientById(f_context.clientId());
    if (!l_client) {
        return;
    }
    // The background lock is enforced by the check_background floor rule.
    const QString l_side = f_context.arguments().join(" ");
    if (auto l_refusal = l_client->changeBackgroundSide(l_side)) {
        f_context.reply(*l_refusal);
        return;
    }
    if (l_side.isEmpty()) {
        f_context.replyToArea(f_context.character() + " unlocked the background side");
    }
    else {
        f_context.replyToArea(f_context.character() + " locked the background side to " + l_side);
    }
}

void cmdLockBackground(CommandContext &f_context)
{
    akashi::Area *l_area = f_context.server()->areaById(f_context.areaId());

    if (l_area->isBgLocked() == false) {
        l_area->toggleBgLock();
    };

    f_context.server()->broadcast(akashi::Packet("CT", {f_context.server()->serverNickname(), f_context.character() + " locked the background.", "1"}), f_context.areaId());
}

void cmdUnlockBackground(CommandContext &f_context)
{
    akashi::Area *l_area = f_context.server()->areaById(f_context.areaId());

    if (l_area->isBgLocked() == true) {
        l_area->toggleBgLock();
    };

    f_context.server()->broadcast(akashi::Packet("CT", {f_context.server()->serverNickname(), f_context.character() + " unlocked the background.", "1"}), f_context.areaId());
}

void cmdStatus(CommandContext &f_context)
{
    akashi::ClientSession *l_client = f_context.server()->clientById(f_context.clientId());
    if (!l_client) {
        return;
    }
    // The verb validates before it commits, so a blocked change needs no
    // rollback and a refused one sends no area update at all.
    if (auto l_refusal = l_client->changeAreaStatus(f_context.argument(0))) {
        f_context.reply(*l_refusal);
    }
}

void cmdJudgeLog(CommandContext &f_context)
{
    akashi::Area *l_area = f_context.server()->areaById(f_context.areaId());
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

void cmdIgnoreBgList(CommandContext &f_context)
{
    akashi::Area *l_area = f_context.server()->areaById(f_context.areaId());
    l_area->toggleIgnoreBgList();
    QString l_state = l_area->ignoreBgList() ? "ignored." : "enforced.";
    f_context.reply("BG list in this area is now " + l_state);
}

void cmdAreaMessage(CommandContext &f_context)
{
    akashi::Area *l_area = f_context.server()->areaById(f_context.areaId());
    if (f_context.argc() == 0) {
        f_context.reply(l_area->areaMessage());
        return;
    }

    l_area->changeAreaMessage(f_context.arguments().join(" "));
    f_context.reply("Updated this area's message.");
}

void cmdToggleMessage(CommandContext &f_context)
{
    akashi::Area *l_area = f_context.server()->areaById(f_context.areaId());
    l_area->toggleAreaMessageJoin();
    QString l_state = l_area->sendAreaMessageOnJoin() ? "enabled." : "disabled.";
    f_context.reply("Sending message on area join is now " + l_state);
}

void cmdClearMessage(CommandContext &f_context)
{
    akashi::Area *l_area = f_context.server()->areaById(f_context.areaId());
    l_area->clearAreaMessage();
    if (l_area->sendAreaMessageOnJoin()) {
        cmdToggleMessage(f_context);
    }
}

void cmdToggleWtce(CommandContext &f_context)
{
    akashi::Area *l_area = f_context.server()->areaById(f_context.areaId());
    l_area->toggleWtceAllowed();
    QString l_state = l_area->isWtceAllowed() ? "enabled." : "disabled.";
    f_context.reply("Using testimony animations is now " + l_state);
}

void cmdToggleShouts(CommandContext &f_context)
{
    akashi::Area *l_area = f_context.server()->areaById(f_context.areaId());
    l_area->toggleShoutAllowed();
    QString l_state = l_area->isShoutAllowed() ? "enabled." : "disabled.";
    f_context.reply("Using shouts is now " + l_state);
}

void cmdFloors(CommandContext &f_context)
{
    if (!f_context.arguments().isEmpty()) {
        cmdFloor(f_context);
        return;
    }
    ServerContext *l_server = f_context.server();
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

void cmdFloor(CommandContext &f_context)
{
    ServerContext *l_server = f_context.server();
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

    akashi::ClientSession *l_client = l_server->clientById(f_context.clientId());
    if (l_client) {
        l_client->changeArea(l_floor->area_ids.first());
    }
}

void cmdCreateArea(CommandContext &f_context)
{
    ServerContext *l_server = f_context.server();
    const QString l_name = f_context.arguments().join(" ");
    const int l_floor_id = l_server->floorIdForArea(f_context.areaId());
    const int l_area_id = l_server->createArea(l_name, l_floor_id);
    if (l_area_id < 0) {
        f_context.reply("That does not look like a usable area name.");
        return;
    }
    f_context.reply("Created area [" + QString::number(l_area_id) + "] " + l_name + " on this floor.");
}

void cmdCreateFloor(CommandContext &f_context)
{
    const QString l_name = f_context.arguments().join(" ");
    const int l_floor_id = f_context.server()->createFloor(l_name);
    if (l_floor_id < 0) {
        f_context.reply("A floor with that name already exists, or the name is unusable.");
        return;
    }
    f_context.reply("Created floor " + l_name + " with one starter area. Use /floor " + l_name + " to visit it.");
}

void cmdRenameArea(CommandContext &f_context)
{
    const QString l_name = f_context.arguments().join(" ");
    if (!f_context.server()->renameArea(f_context.areaId(), l_name)) {
        f_context.reply("That does not look like a usable area name.");
        return;
    }
    f_context.replyToArea("This area is now called " + l_name + ".");
}

void cmdRenameFloor(CommandContext &f_context)
{
    ServerContext *l_server = f_context.server();
    const QString l_name = f_context.arguments().join(" ");
    if (!l_server->renameFloor(l_server->floorIdForArea(f_context.areaId()), l_name)) {
        f_context.reply("A floor with that name already exists, or the name is unusable.");
        return;
    }
    f_context.reply("This floor is now called " + l_name + ".");
}

void cmdRemoveArea(CommandContext &f_context)
{
    ServerContext *l_server = f_context.server();
    bool l_is_id = false;
    int l_area_id = f_context.argument(0).toInt(&l_is_id);
    if (!l_is_id) {
        l_area_id = l_server->areaNames().indexOf(f_context.arguments().join(" "));
    }
    const QString l_name = l_server->areaName(l_area_id);
    if (auto l_error = l_server->removeArea(l_area_id)) {
        f_context.reply(*l_error);
        return;
    }
    f_context.reply("Removed area " + l_name + ".");
}

void cmdRemoveFloor(CommandContext &f_context)
{
    ServerContext *l_server = f_context.server();
    bool l_is_id = false;
    const int l_id = f_context.argument(0).toInt(&l_is_id);
    const akashi::Floor *l_floor = l_is_id ? l_server->floorById(l_id) : l_server->floorByName(f_context.arguments().join(" "));
    if (!l_floor) {
        f_context.reply("No floor found with that name or ID. Use /floors to list them.");
        return;
    }
    const QString l_name = l_floor->name;
    if (auto l_error = l_server->removeFloor(l_floor->id)) {
        f_context.reply(*l_error);
        return;
    }
    f_context.reply("Removed floor " + l_name + " and its areas.");
}

void cmdSaveWorld(CommandContext &f_context)
{
    if (auto l_error = f_context.server()->saveWorld()) {
        f_context.reply("Unable to save: " + *l_error);
        return;
    }
    f_context.reply("Saved the floors, areas and their rules to areas.json.");
}

void cmdLoadWorld(CommandContext &f_context)
{
    if (auto l_error = f_context.server()->reloadWorld()) {
        f_context.reply("Unable to reload: " + *l_error);
        return;
    }
    f_context.replyToServer("The floors and areas were reloaded from areas.json.");
}

void cmdSaveFloor(CommandContext &f_context)
{
    ServerContext *l_server = f_context.server();
    int l_floor_id = l_server->floorIdForArea(f_context.areaId());
    if (f_context.argc() > 0) {
        bool l_is_id = false;
        const int l_id = f_context.argument(0).toInt(&l_is_id);
        const akashi::Floor *l_floor = l_is_id ? l_server->floorById(l_id) : l_server->floorByName(f_context.arguments().join(" "));
        if (!l_floor) {
            f_context.reply("No floor found with that name or ID. Use /floors to list them.");
            return;
        }
        l_floor_id = l_floor->id;
    }
    const QString l_name = l_server->floorById(l_floor_id)->name;
    if (auto l_error = l_server->saveFloor(l_floor_id)) {
        f_context.reply("Unable to save: " + *l_error);
        return;
    }
    f_context.reply("Saved floor " + l_name + " to config/floors. Load it with /loadfloor " + l_name + ".");
}

void cmdLoadFloor(CommandContext &f_context)
{
    const QString l_name = f_context.arguments().join(" ");
    if (auto l_error = f_context.server()->loadFloor(l_name)) {
        f_context.reply("Unable to load: " + *l_error);
        return;
    }
    f_context.reply("Loaded floor " + l_name + " from its file.");
}

void cmdWebfiles(CommandContext &f_context)
{
    const QVector<akashi::ClientSession *> l_clients = f_context.server()->clients();
    QStringList l_weblinks;
    for (akashi::ClientSession *l_client : l_clients) {
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
        {QStringLiteral("getarea"), {QStringLiteral("ga")}, {akashi::permission::user}, 0, QStringLiteral("/getarea"), QStringLiteral("Lists all clients in the area you are in.")},
        cmdGetArea, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("getareas"), {QStringLiteral("gas")}, {akashi::permission::user}, 0, QStringLiteral("/getareas"), QStringLiteral("Lists all clients in all areas.")},
        cmdGetAreas, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("area"), {}, {akashi::permission::user}, 1, QStringLiteral("/area <id>"), QStringLiteral("Moves you to the area with the given ID.")},
        cmdArea, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("cm"), {}, {akashi::permission::user}, 0, QStringLiteral("/cm [id]"), QStringLiteral("Claims CM or adds another client as CM.")},
        cmdCm, QStringLiteral("core"));

    CommandSpec l_uncm;
    l_uncm.name = QStringLiteral("uncm");
    l_uncm.usage = QStringLiteral("/uncm [id|all]");
    l_uncm.description = QStringLiteral("Removes CM status from yourself or another client.");
    l_uncm.variants = {
        {QStringLiteral("own"), 0, 0, {akashi::permission::gamemaster}, QStringLiteral("/uncm"), QStringLiteral("Removes your own CM status."), cmdUncmOwn},
        {QStringLiteral("other"), 1, -1, {akashi::permission::remove_gamemaster}, QStringLiteral("/uncm <id|all>"), QStringLiteral("Removes another client's CM status, or every CM's except yours."), cmdUncmOther},
    };
    f_registry.registerCommand(l_uncm, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("invite"), {}, {akashi::permission::gamemaster}, 1, QStringLiteral("/invite <id>"), QStringLiteral("Invites a client to the area.")},
        cmdInvite, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("uninvite"), {}, {akashi::permission::gamemaster}, 1, QStringLiteral("/uninvite <id>"), QStringLiteral("Removes a client from the area invite list.")},
        cmdUninvite, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("area_lock"), {QStringLiteral("lock_area"), QStringLiteral("lock")}, {akashi::permission::gamemaster}, 0, QStringLiteral("/area_lock"), QStringLiteral("Locks the area so only invited clients may enter.")},
        cmdAreaLock, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("area_spectate"), {QStringLiteral("spectatable")}, {akashi::permission::gamemaster}, 0, QStringLiteral("/area_spectate"), QStringLiteral("Sets the area to spectate-only mode.")},
        cmdAreaSpectate, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("area_unlock"), {QStringLiteral("unlock_area"), QStringLiteral("unlock")}, {akashi::permission::gamemaster}, 0, QStringLiteral("/area_unlock"), QStringLiteral("Unlocks the area.")},
        cmdAreaUnlock, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("area_kick"), {QStringLiteral("kick_area"), QStringLiteral("areakick")}, {akashi::permission::gamemaster}, 1, QStringLiteral("/area_kick <id|all> [area]"), QStringLiteral("Kicks a client or all non-CMs from the area.")},
        cmdAreaKick, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("background"), {QStringLiteral("bg")}, {akashi::permission::user}, 1, QStringLiteral("/background <name>"), QStringLiteral("Changes the background of the area.")},
        cmdBackground, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("side"), {}, {akashi::permission::gamemaster}, 0, QStringLiteral("/side [name]"), QStringLiteral("Locks or unlocks the background side.")},
        cmdSide, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("lock_background"), {QStringLiteral("lock_bg"), QStringLiteral("lockbg"), QStringLiteral("bglock")}, {akashi::permission::gamemaster}, 0, QStringLiteral("/lock_background"), QStringLiteral("Locks the background in the area.")},
        cmdLockBackground, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("unlock_background"), {QStringLiteral("unlock_bg"), QStringLiteral("unlockbg"), QStringLiteral("bgunlock")}, {akashi::permission::gamemaster}, 0, QStringLiteral("/unlock_background"), QStringLiteral("Unlocks the background in the area.")},
        cmdUnlockBackground, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("status"), {}, {akashi::permission::user}, 1, QStringLiteral("/status <status>"), QStringLiteral("Changes the status of the area.")},
        cmdStatus, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("judgelog"), {}, {akashi::permission::gamemaster}, 0, QStringLiteral("/judgelog"), QStringLiteral("Displays the judge log for the area.")},
        cmdJudgeLog, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("ignore_bglist"), {QStringLiteral("ignorebglist")}, {akashi::permission::ignore_background_list}, 0, QStringLiteral("/ignore_bglist"), QStringLiteral("Toggles whether the background list is enforced.")},
        cmdIgnoreBgList, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("areamessage"), {}, {akashi::permission::gamemaster}, 0, QStringLiteral("/areamessage [message]"), QStringLiteral("Views or sets the area message.")},
        cmdAreaMessage, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("togglemessage"), {}, {akashi::permission::gamemaster}, 0, QStringLiteral("/togglemessage"), QStringLiteral("Toggles sending the area message on join.")},
        cmdToggleMessage, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("clearmessage"), {}, {akashi::permission::gamemaster}, 0, QStringLiteral("/clearmessage"), QStringLiteral("Clears the area message.")},
        cmdClearMessage, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("toggle_wtce"), {QStringLiteral("togglewtce")}, {akashi::permission::gamemaster}, 0, QStringLiteral("/toggle_wtce"), QStringLiteral("Toggles testimony animations in the area.")},
        cmdToggleWtce, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("toggle_shouts"), {QStringLiteral("toggleshouts")}, {akashi::permission::gamemaster}, 0, QStringLiteral("/toggle_shouts"), QStringLiteral("Toggles shouts in the area.")},
        cmdToggleShouts, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("floors"), {}, {akashi::permission::user}, 0, QStringLiteral("/floors"), QStringLiteral("Lists all floors and their areas.")},
        cmdFloors, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("floor"), {}, {akashi::permission::user}, 1, QStringLiteral("/floor <name|id>"), QStringLiteral("Moves you to the first area on the given floor.")},
        cmdFloor, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("createarea"), {}, {akashi::permission::modify_floors}, 1, QStringLiteral("/createarea <name>"), QStringLiteral("Creates a new area on this floor.")},
        cmdCreateArea, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("createfloor"), {}, {akashi::permission::modify_floors}, 1, QStringLiteral("/createfloor <name>"), QStringLiteral("Creates a new floor with one starter area.")},
        cmdCreateFloor, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("renamearea"), {}, {akashi::permission::modify_floors}, 1, QStringLiteral("/renamearea <name>"), QStringLiteral("Renames the area you are in.")},
        cmdRenameArea, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("renamefloor"), {}, {akashi::permission::modify_floors}, 1, QStringLiteral("/renamefloor <name>"), QStringLiteral("Renames the floor you are on.")},
        cmdRenameFloor, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("removearea"), {}, {akashi::permission::modify_floors}, 1, QStringLiteral("/removearea <id|name>"), QStringLiteral("Removes an empty area; later area IDs shift down.")},
        cmdRemoveArea, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("removefloor"), {}, {akashi::permission::modify_floors}, 1, QStringLiteral("/removefloor <id|name>"), QStringLiteral("Removes a floor whose areas are all empty.")},
        cmdRemoveFloor, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("saveworld"), {QStringLiteral("saveareas")}, {akashi::permission::modify_floors}, 0, QStringLiteral("/saveworld"), QStringLiteral("Saves every floor, area and their rules to areas.json.")},
        cmdSaveWorld, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("loadworld"), {QStringLiteral("loadareas")}, {akashi::permission::modify_floors}, 0, QStringLiteral("/loadworld"), QStringLiteral("Rebuilds every floor and area from areas.json.")},
        cmdLoadWorld, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("savefloor"), {}, {akashi::permission::modify_floors}, 0, QStringLiteral("/savefloor [name|id]"), QStringLiteral("Saves one floor to its own file under config/floors.")},
        cmdSaveFloor, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("loadfloor"), {}, {akashi::permission::modify_floors}, 1, QStringLiteral("/loadfloor <name>"), QStringLiteral("Loads a floor file, replacing a same-named floor or adding a new one.")},
        cmdLoadFloor, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("webfiles"), {}, {akashi::permission::user}, 0, QStringLiteral("/webfiles"), QStringLiteral("Lists download links for iniswapped characters.")},
        cmdWebfiles, QStringLiteral("core"));
}

} // namespace akashi::commands
