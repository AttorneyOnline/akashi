#include "commands/messaging_commands.h"

#include "akashi/permissions.h"
#include "aoclient.h"
#include "area_data.h"
#include "core/command_context.h"
#include "core/command_registry.h"
#include "core/command_spec.h"
#include "proto/packet.h"
#include "server.h"

namespace akashi::commands {

static QString reprimand(Server *f_server, bool f_positive = false)
{
    if (f_positive)
        return f_server->praiseList().at(CommandContext::genRand(0, f_server->praiseList().size() - 1));
    else
        return f_server->reprimandsList().at(CommandContext::genRand(0, f_server->reprimandsList().size() - 1));
}

static void handlePos(CommandContext &f_context)
{
    AOClient *l_self = f_context.server()->clientById(f_context.clientId());
    l_self->changePosition(f_context.argument(0));
    l_self->updateEvidenceList(f_context.server()->areaById(f_context.areaId()));
}

static void handleForcePos(CommandContext &f_context)
{
    bool ok;
    QList<AOClient *> l_targets;
    int l_target_id = f_context.argument(1).toInt(&ok);
    int l_forced_clients = 0;
    if (!ok && f_context.argument(1) != "*") {
        f_context.reply("That does not look like a valid ID.");
        return;
    }
    else if (ok) {
        AOClient *l_target_client = f_context.server()->clientById(l_target_id);
        if (l_target_client != nullptr)
            l_targets.append(l_target_client);
        else {
            f_context.reply("Target ID not found!");
            return;
        }
    }
    else if (f_context.argument(1) == "*") {
        const QVector<AOClient *> l_clients = f_context.server()->clients();
        for (AOClient *l_client : l_clients) {
            if (l_client->areaId() == f_context.areaId())
                l_targets.append(l_client);
        }
    }
    for (AOClient *l_target : l_targets) {
        l_target->sendServerMessage("Position forcibly changed by CM.");
        l_target->changePosition(f_context.argument(0));
        l_forced_clients++;
    }
    f_context.reply("Forced " + QString::number(l_forced_clients) + " into pos " + f_context.argument(0) + ".");
}

static void handleG(CommandContext &f_context)
{
    QString l_sender_name = f_context.name();
    QString l_sender_area = f_context.areaName();
    QString l_sender_message = f_context.arguments().join(" ");
    akashi::Packet l_mod_packet("CT", {"[G][" + f_context.ipid() + "][" + l_sender_area + "]" + l_sender_name, l_sender_message});
    akashi::Packet l_user_packet("CT", {"[G][" + l_sender_area + "]" + l_sender_name, l_sender_message});
    f_context.server()->broadcast(l_user_packet, l_mod_packet, Server::TARGET_TYPE::AUTHENTICATED);
}

static void handleNeed(CommandContext &f_context)
{
    QString l_sender_area = f_context.areaName();
    QString l_sender_message = f_context.arguments().join(" ");
    f_context.server()->broadcast(akashi::Packet("CT", {f_context.server()->serverNickname(), "=== Advert ===\n[" + l_sender_area + "] needs " + l_sender_message + "."}), Server::TARGET_TYPE::ADVERT);
}

static void handleSwitch(CommandContext &f_context)
{
    AOClient *l_self = f_context.server()->clientById(f_context.clientId());
    int l_selected_char_id = f_context.server()->characterId(f_context.arguments().join(" "));
    if (l_selected_char_id == -1) {
        f_context.reply("That does not look like a valid character.");
        return;
    }
    if (l_self->changeCharacter(l_selected_char_id)) {
        l_self->setCharacterId(l_selected_char_id);
    }
    else {
        f_context.reply("The character you picked is either taken or invalid.");
    }
}

static void handleRandomChar(CommandContext &f_context)
{
    AOClient *l_self = f_context.server()->clientById(f_context.clientId());
    AreaData *l_area = f_context.server()->areaById(f_context.areaId());
    int l_selected_char_id;
    bool l_taken = true;
    while (l_taken) {
        l_selected_char_id = CommandContext::genRand(0, f_context.server()->characterCount() - 1);
        if (!l_area->charactersTaken().contains(l_selected_char_id)) {
            l_taken = false;
        }
    }
    if (l_self->changeCharacter(l_selected_char_id)) {
        l_self->setCharacterId(l_selected_char_id);
    }
}

static void handleToggleGlobal(CommandContext &f_context)
{
    AOClient *l_self = f_context.server()->clientById(f_context.clientId());
    l_self->setGlobalEnabled(!l_self->isGlobalEnabled());
    QString l_str_en = l_self->isGlobalEnabled() ? "shown" : "hidden";
    f_context.reply("Global chat set to " + l_str_en);
}

static void handlePM(CommandContext &f_context)
{
    bool ok;
    int l_target_id = f_context.argument(0).toInt(&ok);
    if (!ok) {
        f_context.reply("That does not look like a valid ID.");
        return;
    }
    AOClient *l_target_client = f_context.server()->clientById(l_target_id);
    if (l_target_client == nullptr) {
        f_context.reply("No client with that ID found.");
        return;
    }
    if (l_target_client->isPmMuted()) {
        f_context.reply("That user is not recieving PMs.");
        return;
    }
    QStringList l_rest = f_context.arguments().mid(1);
    QString l_message = l_rest.join(" ");
    l_target_client->sendServerMessage("Message from " + f_context.name() + " (" + QString::number(f_context.clientId()) + "): " + l_message);
    f_context.reply("PM sent to " + QString::number(l_target_id) + ". Message: " + l_message);
}

static void handleAnnounce(CommandContext &f_context)
{
    f_context.replyToServer("=== Announcement ===\r\n" + f_context.arguments().join(" ") + "\r\n=============");
}

static void handleM(CommandContext &f_context)
{
    QString l_sender_name = f_context.name();
    QString l_sender_message = f_context.arguments().join(" ");
    f_context.server()->broadcast(akashi::Packet("CT", {"[M]" + l_sender_name, l_sender_message}), Server::TARGET_TYPE::MODCHAT);
}

static void handleGM(CommandContext &f_context)
{
    QString l_sender_name = f_context.name();
    QString l_sender_area = f_context.areaName();
    QString l_sender_message = f_context.arguments().join(" ");
    f_context.server()->broadcast(akashi::Packet("CT", {"[G][" + l_sender_area + "]" + "[" + l_sender_name + "][M]", l_sender_message}), Server::TARGET_TYPE::MODCHAT);
}

static void handleLM(CommandContext &f_context)
{
    QString l_sender_name = f_context.name();
    QString l_sender_message = f_context.arguments().join(" ");
    f_context.server()->broadcast(akashi::Packet("CT", {"[" + l_sender_name + "][M]", l_sender_message}), f_context.areaId());
}

static void handleGimp(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        if (l_target->hasSanction(sanction::gimped))
            f_context.reply("That player is already gimped!");
        else {
            f_context.reply("Gimped player.");
            l_target->reply("You have been gimped! " + reprimand(f_context.server()));
        }
        l_target->setSanction(sanction::gimped, true);
    }
}

static void handleUngimp(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        if (!l_target->hasSanction(sanction::gimped))
            f_context.reply("That player is not gimped!");
        else {
            f_context.reply("Ungimped player.");
            l_target->reply("A moderator has ungimped you! " + reprimand(f_context.server(), true));
        }
        l_target->setSanction(sanction::gimped, false);
    }
}

static void handleDisemvowel(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        if (l_target->hasSanction(sanction::disemvoweled))
            f_context.reply("That player is already disemvoweled!");
        else {
            f_context.reply("Disemvoweled player.");
            l_target->reply("You have been disemvoweled! " + reprimand(f_context.server()));
        }
        l_target->setSanction(sanction::disemvoweled, true);
    }
}

static void handleUnDisemvowel(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        if (!l_target->hasSanction(sanction::disemvoweled))
            f_context.reply("That player is not disemvoweled!");
        else {
            f_context.reply("Undisemvoweled player.");
            l_target->reply("A moderator has undisemvoweled you! " + reprimand(f_context.server(), true));
        }
        l_target->setSanction(sanction::disemvoweled, false);
    }
}

static void handleShake(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        if (l_target->hasSanction(sanction::shaken))
            f_context.reply("That player is already shaken!");
        else {
            f_context.reply("Shook player.");
            l_target->reply("A moderator has shaken your words! " + reprimand(f_context.server()));
        }
        l_target->setSanction(sanction::shaken, true);
    }
}

static void handleUnShake(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        if (!l_target->hasSanction(sanction::shaken))
            f_context.reply("That player is not shaken!");
        else {
            f_context.reply("Unshook player.");
            l_target->reply("A moderator has unshook you! " + reprimand(f_context.server(), true));
        }
        l_target->setSanction(sanction::shaken, false);
    }
}

static void handleMutePM(CommandContext &f_context)
{
    AOClient *l_self = f_context.server()->clientById(f_context.clientId());
    l_self->setPmMuted(!l_self->isPmMuted());
    QString l_str_en = l_self->isPmMuted() ? "muted" : "unmuted";
    f_context.reply("PM's are now " + l_str_en);
}

static void handleToggleAdverts(CommandContext &f_context)
{
    AOClient *l_self = f_context.server()->clientById(f_context.clientId());
    l_self->setAdvertEnabled(!l_self->isAdvertEnabled());
    QString l_str_en = l_self->isAdvertEnabled() ? "on" : "off";
    f_context.reply("Advertisements turned " + l_str_en);
}

static void handleAfk(CommandContext &f_context)
{
    AOClient *l_self = f_context.server()->clientById(f_context.clientId());
    l_self->setAfk(true);
    f_context.reply("You are now AFK.");
    l_self->setCharacterName(l_self->characterName() + " [AFK]");
}

static void handleCharCurse(CommandContext &f_context)
{
    bool conv_ok = false;
    int l_uid = f_context.argument(0).toInt(&conv_ok);
    if (!conv_ok) {
        f_context.reply("Invalid user ID.");
        return;
    }

    AOClient *l_target = f_context.server()->clientById(l_uid);
    if (l_target == nullptr) {
        f_context.reply("No client with that ID found.");
        return;
    }

    if (l_target->isCharCursed()) {
        f_context.reply("That player is already charcursed!");
        return;
    }

    if (f_context.argc() == 1) {
        l_target->addCharCurse(f_context.server()->characterId(l_target->character()));
    }
    else {
        QStringList l_argv = f_context.arguments();
        l_argv.removeFirst();
        QStringList l_char_names = l_argv.join(" ").split(",");

        l_target->clearCharCurse();
        for (const QString &l_char_name : qAsConst(l_char_names)) {
            int char_id = f_context.server()->characterId(l_char_name);
            if (char_id == -1) {
                f_context.reply("Could not find character: " + l_char_name);
                return;
            }
            l_target->addCharCurse(char_id);
        }
    }

    l_target->setCharCursed(true);

    if (!l_target->charCurseList().contains(f_context.server()->characterId(l_target->character()))) {
        l_target->changeCharacter(-1);
        f_context.server()->updateCharsTaken(f_context.server()->areaById(f_context.areaId()));
        l_target->sendPacket("DONE");
    }
    else {
        f_context.server()->updateCharsTaken(f_context.server()->areaById(f_context.areaId()));
    }

    l_target->sendServerMessage("You have been charcursed!");
    f_context.reply("Charcursed player.");
}

static void handleUnCharCurse(CommandContext &f_context)
{
    bool conv_ok = false;
    int l_uid = f_context.argument(0).toInt(&conv_ok);
    if (!conv_ok) {
        f_context.reply("Invalid user ID.");
        return;
    }

    AOClient *l_target = f_context.server()->clientById(l_uid);
    if (l_target == nullptr) {
        f_context.reply("No client with that ID found.");
        return;
    }

    if (!l_target->isCharCursed()) {
        f_context.reply("That player is not charcursed!");
        return;
    }
    l_target->setCharCursed(false);
    l_target->clearCharCurse();
    f_context.server()->updateCharsTaken(f_context.server()->areaById(f_context.areaId()));
    f_context.reply("Uncharcursed player.");
    l_target->sendServerMessage("You were uncharcursed.");
}

static void handleCharSelect(CommandContext &f_context)
{
    AOClient *l_self = f_context.server()->clientById(f_context.clientId());
    l_self->changeCharacter(-1);
    f_context.sendPacket("DONE", {});
}

static void handleForceCharSelect(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        l_target->forceCharacterSelect();
        f_context.reply("Client has been forced into character select!");
    }
}

static void handleA(CommandContext &f_context)
{
    bool ok;
    int l_area_id = f_context.argument(0).toInt(&ok);
    if (!ok) {
        f_context.reply("This does not look like a valid AreaID.");
        return;
    }

    AreaData *l_area = f_context.server()->areaById(l_area_id);
    if (!l_area->owners().contains(f_context.clientId())) {
        f_context.reply("You are not CM in that area.");
        return;
    }

    QStringList l_argv = f_context.arguments();
    l_argv.removeAt(0);
    QString l_sender_name = f_context.name();
    QString l_ooc_message = l_argv.join(" ");
    f_context.server()->broadcast(akashi::Packet("CT", {"[CM]" + l_sender_name, l_ooc_message}), l_area_id);
}

static void handleS(CommandContext &f_context)
{
    int l_all_areas = f_context.server()->areaCount() - 1;
    QString l_sender_name = f_context.name();
    QString l_ooc_message = f_context.arguments().join(" ");

    for (int i = 0; i <= l_all_areas; i++) {
        if (f_context.server()->areaById(i)->owners().contains(f_context.clientId()))
            f_context.server()->broadcast(akashi::Packet("CT", {"[CM]" + l_sender_name, l_ooc_message}), i);
    }
}

static void handleFirstPerson(CommandContext &f_context)
{
    AOClient *l_self = f_context.server()->clientById(f_context.clientId());
    l_self->setFirstPerson(!l_self->isFirstPerson());
    QString l_str_en = l_self->isFirstPerson() ? "enabled" : "disabled";
    f_context.reply("First person mode " + l_str_en + ".");
}

void registerMessagingCommands(CommandRegistry &f_registry)
{
    f_registry.registerCommand({"pos", {}, {}, 1}, handlePos, "core");
    f_registry.registerCommand({"forcepos", {}, {permission::gamemaster}, 2}, handleForcePos, "core");
    f_registry.registerCommand({"g", {}, {}, 1}, handleG, "core");
    f_registry.registerCommand({"need", {}, {}, 1}, handleNeed, "core");
    f_registry.registerCommand({"switch", {}, {}, 1}, handleSwitch, "core");
    f_registry.registerCommand({"randomchar", {}, {}, 0}, handleRandomChar, "core");
    f_registry.registerCommand({"toggleglobal", {}, {}, 0}, handleToggleGlobal, "core");
    f_registry.registerCommand({"pm", {}, {}, 2}, handlePM, "core");
    f_registry.registerCommand({"announce", {}, {permission::announcer}, 1}, handleAnnounce, "core");
    f_registry.registerCommand({"m", {}, {permission::chat_moderator}, 1}, handleM, "core");
    f_registry.registerCommand({"gm", {}, {permission::chat_moderator}, 1}, handleGM, "core");
    f_registry.registerCommand({"lm", {}, {permission::chat_moderator}, 1}, handleLM, "core");
    f_registry.registerCommand({"gimp", {}, {permission::mute}, 1}, handleGimp, "core");
    f_registry.registerCommand({"ungimp", {}, {permission::mute}, 1}, handleUngimp, "core");
    f_registry.registerCommand({"disemvowel", {}, {permission::mute}, 1}, handleDisemvowel, "core");
    f_registry.registerCommand({"undisemvowel", {}, {permission::mute}, 1}, handleUnDisemvowel, "core");
    f_registry.registerCommand({"shake", {}, {permission::mute}, 1}, handleShake, "core");
    f_registry.registerCommand({"unshake", {}, {permission::mute}, 1}, handleUnShake, "core");
    f_registry.registerCommand({"mutepm", {}, {}, 0}, handleMutePM, "core");
    f_registry.registerCommand({"toggleadverts", {}, {}, 0}, handleToggleAdverts, "core");
    f_registry.registerCommand({"afk", {}, {}, 0}, handleAfk, "core");
    f_registry.registerCommand({"charcurse", {}, {permission::mute}, 1}, handleCharCurse, "core");
    f_registry.registerCommand({"uncharcurse", {}, {permission::mute}, 1}, handleUnCharCurse, "core");
    f_registry.registerCommand({"charselect", {}, {}, 0}, handleCharSelect, "core");
    f_registry.registerCommand({"force_charselect", {"forcecharselect"}, {permission::force_charselect}, 1}, handleForceCharSelect, "core");
    f_registry.registerCommand({"a", {}, {}, 2}, handleA, "core");
    f_registry.registerCommand({"s", {}, {}, 0}, handleS, "core");
    f_registry.registerCommand({"firstperson", {}, {}, 0}, handleFirstPerson, "core");
}

} // namespace akashi::commands
