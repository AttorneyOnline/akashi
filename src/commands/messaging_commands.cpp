#include "commands/messaging_commands.h"

#include "akashi/permissions.h"
#include "commands/moderation_commands.h"
#include "core/client_session.h"
#include "core/command_context.h"
#include "core/command_registry.h"
#include "core/command_spec.h"
#include "core/server_context.h"
#include "proto/packet.h"
#include "proto/text_utils.h"
#include "world/area.h"

#include <utility>

namespace akashi::commands {

static QString reprimand(ServerContext *f_server, bool f_positive = false)
{
    if (f_positive)
        return f_server->praiseList().at(CommandContext::genRand(0, f_server->praiseList().size() - 1));
    else
        return f_server->reprimandsList().at(CommandContext::genRand(0, f_server->reprimandsList().size() - 1));
}

void cmdPos(CommandContext &f_context)
{
    akashi::ClientSession *l_self = f_context.server()->clientById(f_context.clientId());
    l_self->changePosition(f_context.argument(0));
}

void cmdForcePos(CommandContext &f_context)
{
    bool ok;
    QList<akashi::ClientSession *> l_targets;
    int l_target_id = f_context.argument(1).toInt(&ok);
    int l_forced_clients = 0;
    if (!ok && f_context.argument(1) != "*") {
        f_context.reply("That does not look like a valid ID.");
        return;
    }
    else if (ok) {
        akashi::ClientSession *l_target_client = f_context.server()->clientById(l_target_id);
        if (l_target_client != nullptr)
            l_targets.append(l_target_client);
        else {
            f_context.reply("Target ID not found!");
            return;
        }
    }
    else if (f_context.argument(1) == "*") {
        const QVector<akashi::ClientSession *> l_clients = f_context.server()->clients();
        for (akashi::ClientSession *l_client : l_clients) {
            if (l_client->areaId() == f_context.areaId())
                l_targets.append(l_client);
        }
    }
    for (akashi::ClientSession *l_target : l_targets) {
        l_target->sendServerMessage("Position forcibly changed by CM.");
        l_target->changePosition(f_context.argument(0));
        l_forced_clients++;
    }
    // The summary echoes what was actually stored, not the raw argument.
    f_context.reply("Forced " + QString::number(l_forced_clients) + " into pos " + akashi::sanitizePosition(f_context.argument(0)) + ".");
}

void cmdG(CommandContext &f_context)
{
    QString l_sender_name = f_context.name();
    QString l_sender_area = f_context.areaName();
    QString l_sender_message = f_context.arguments().join(" ");
    akashi::Packet l_mod_packet("CT", {"[G][" + f_context.ipid() + "][" + l_sender_area + "]" + l_sender_name, l_sender_message});
    akashi::Packet l_user_packet("CT", {"[G][" + l_sender_area + "]" + l_sender_name, l_sender_message});
    f_context.server()->broadcast(l_user_packet, l_mod_packet, ServerContext::TARGET_TYPE::AUTHENTICATED);
}

void cmdNeed(CommandContext &f_context)
{
    QString l_sender_area = f_context.areaName();
    QString l_sender_message = f_context.arguments().join(" ");
    f_context.server()->broadcast(akashi::Packet("CT", {f_context.server()->serverNickname(), "=== Advert ===\n[" + l_sender_area + "] needs " + l_sender_message + "."}), ServerContext::TARGET_TYPE::ADVERT);
}

void cmdSwitch(CommandContext &f_context)
{
    akashi::ClientSession *l_self = f_context.server()->clientById(f_context.clientId());
    int l_selected_char_id = f_context.server()->characterId(f_context.arguments().join(" "));
    if (l_selected_char_id == -1) {
        f_context.reply("That does not look like a valid character.");
        return;
    }
    if (auto l_refused = l_self->takeCharacter(l_selected_char_id)) {
        // A rule speaks its own reason; the mechanism refuses silently and
        // keeps this door's traditional answer.
        f_context.reply(l_refused->isEmpty() ? QStringLiteral("The character you picked is either taken or invalid.") : *l_refused);
    }
}

void cmdRandomChar(CommandContext &f_context)
{
    akashi::ClientSession *l_self = f_context.server()->clientById(f_context.clientId());
    akashi::Area *l_area = f_context.server()->areaById(f_context.areaId());

    // Collect the free characters in one pass. The old retry loop drew random
    // ids until one was free, which never terminated once every character was
    // taken - one command could hang the whole single-threaded server.
    const QList<int> l_taken = l_area->charactersTaken();
    QList<int> l_free;
    const int l_count = f_context.server()->characterCount();
    for (int i = 0; i < l_count; i++) {
        if (!l_taken.contains(i)) {
            l_free.append(i);
        }
    }
    if (l_free.isEmpty()) {
        f_context.reply("Every character in this area is already taken.");
        return;
    }

    const int l_selected_char_id = l_free.at(CommandContext::genRand(0, l_free.size() - 1));
    if (auto l_refused = l_self->takeCharacter(l_selected_char_id); l_refused && !l_refused->isEmpty()) {
        f_context.reply(*l_refused);
    }
}

void cmdToggleGlobal(CommandContext &f_context)
{
    akashi::ClientSession *l_self = f_context.server()->clientById(f_context.clientId());
    l_self->setGlobalEnabled(!l_self->isGlobalEnabled());
    QString l_str_en = l_self->isGlobalEnabled() ? "shown" : "hidden";
    f_context.reply("Global chat set to " + l_str_en);
}

void cmdPM(CommandContext &f_context)
{
    bool ok;
    int l_target_id = f_context.argument(0).toInt(&ok);
    if (!ok) {
        f_context.reply("That does not look like a valid ID.");
        return;
    }
    akashi::ClientSession *l_target_client = f_context.server()->clientById(l_target_id);
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

void cmdAnnounce(CommandContext &f_context)
{
    f_context.replyToServer("=== Announcement ===\r\n" + f_context.arguments().join(" ") + "\r\n=============");
}

void cmdM(CommandContext &f_context)
{
    QString l_sender_name = f_context.name();
    QString l_sender_message = f_context.arguments().join(" ");
    f_context.server()->broadcast(akashi::Packet("CT", {"[M]" + l_sender_name, l_sender_message}), ServerContext::TARGET_TYPE::MODCHAT);
}

void cmdGM(CommandContext &f_context)
{
    QString l_sender_name = f_context.name();
    QString l_sender_area = f_context.areaName();
    QString l_sender_message = f_context.arguments().join(" ");
    f_context.server()->broadcast(akashi::Packet("CT", {"[G][" + l_sender_area + "]" + "[" + l_sender_name + "][M]", l_sender_message}), ServerContext::TARGET_TYPE::MODCHAT);
}

void cmdLM(CommandContext &f_context)
{
    QString l_sender_name = f_context.name();
    QString l_sender_message = f_context.arguments().join(" ");
    f_context.server()->broadcast(akashi::Packet("CT", {"[" + l_sender_name + "][M]", l_sender_message}), f_context.areaId());
}

void cmdGimp(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        if (!applySanctionSchedule(f_context, *l_target, sanction::gimped))
            return;
        if (l_target->hasSanction(sanction::gimped))
            f_context.reply("That player is already gimped!");
        else {
            f_context.reply("Gimped player.");
            l_target->reply("You have been gimped! " + reprimand(f_context.server()));
        }
        l_target->setSanction(sanction::gimped, true);
    }
}

void cmdUngimp(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        clearSanctionSchedule(f_context, *l_target, sanction::gimped);
        if (!l_target->hasSanction(sanction::gimped))
            f_context.reply("That player is not gimped!");
        else {
            f_context.reply("Ungimped player.");
            l_target->reply("A moderator has ungimped you! " + reprimand(f_context.server(), true));
        }
        l_target->setSanction(sanction::gimped, false);
    }
}

void cmdDisemvowel(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        if (!applySanctionSchedule(f_context, *l_target, sanction::disemvoweled))
            return;
        if (l_target->hasSanction(sanction::disemvoweled))
            f_context.reply("That player is already disemvoweled!");
        else {
            f_context.reply("Disemvoweled player.");
            l_target->reply("You have been disemvoweled! " + reprimand(f_context.server()));
        }
        l_target->setSanction(sanction::disemvoweled, true);
    }
}

void cmdUnDisemvowel(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        clearSanctionSchedule(f_context, *l_target, sanction::disemvoweled);
        if (!l_target->hasSanction(sanction::disemvoweled))
            f_context.reply("That player is not disemvoweled!");
        else {
            f_context.reply("Undisemvoweled player.");
            l_target->reply("A moderator has undisemvoweled you! " + reprimand(f_context.server(), true));
        }
        l_target->setSanction(sanction::disemvoweled, false);
    }
}

void cmdShake(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        if (!applySanctionSchedule(f_context, *l_target, sanction::shaken))
            return;
        if (l_target->hasSanction(sanction::shaken))
            f_context.reply("That player is already shaken!");
        else {
            f_context.reply("Shook player.");
            l_target->reply("A moderator has shaken your words! " + reprimand(f_context.server()));
        }
        l_target->setSanction(sanction::shaken, true);
    }
}

void cmdUnShake(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        clearSanctionSchedule(f_context, *l_target, sanction::shaken);
        if (!l_target->hasSanction(sanction::shaken))
            f_context.reply("That player is not shaken!");
        else {
            f_context.reply("Unshook player.");
            l_target->reply("A moderator has unshook you! " + reprimand(f_context.server(), true));
        }
        l_target->setSanction(sanction::shaken, false);
    }
}

void cmdMedieval(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        if (!applySanctionSchedule(f_context, *l_target, sanction::medieval))
            return;
        if (l_target->hasSanction(sanction::medieval))
            f_context.reply("That player is already speaking Ye Olde English!");
        else {
            f_context.reply("It is done, sire.");
            l_target->reply("Forsooth! Thine speech will henceforth be Ye Olde!");
        }
        l_target->setSanction(sanction::medieval, true);
    }
}

void cmdUnmedieval(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        clearSanctionSchedule(f_context, *l_target, sanction::medieval);
        if (!l_target->hasSanction(sanction::medieval))
            f_context.reply("That player is not speaking Ye Olde English!");
        else {
            f_context.reply("Un-medieval'd player.");
            l_target->reply("Hark! Thine speech hast been returneth to normal.");
        }
        l_target->setSanction(sanction::medieval, false);
    }
}

void cmdMedievalMode(CommandContext &f_context)
{
    akashi::Area *l_area = f_context.area();
    l_area->toggleMedievalMode();
    const QString l_state = l_area->isMedievalMode() ? "enabled." : "disabled.";
    f_context.replyToArea("Hear ye, hear ye! Medieval Mode is now " + l_state);
}

void cmdMutePM(CommandContext &f_context)
{
    akashi::ClientSession *l_self = f_context.server()->clientById(f_context.clientId());
    l_self->setPmMuted(!l_self->isPmMuted());
    QString l_str_en = l_self->isPmMuted() ? "muted" : "unmuted";
    f_context.reply("PM's are now " + l_str_en);
}

void cmdToggleAdverts(CommandContext &f_context)
{
    akashi::ClientSession *l_self = f_context.server()->clientById(f_context.clientId());
    l_self->setAdvertEnabled(!l_self->isAdvertEnabled());
    QString l_str_en = l_self->isAdvertEnabled() ? "on" : "off";
    f_context.reply("Advertisements turned " + l_str_en);
}

void cmdAfk(CommandContext &f_context)
{
    akashi::ClientSession *l_self = f_context.server()->clientById(f_context.clientId());
    l_self->setAfk(true);
    f_context.reply("You are now AFK.");
    l_self->setCharacterName(l_self->characterName() + " [AFK]");
}

void cmdCharCurse(CommandContext &f_context)
{
    bool conv_ok = false;
    int l_uid = f_context.argument(0).toInt(&conv_ok);
    if (!conv_ok) {
        f_context.reply("Invalid user ID.");
        return;
    }

    akashi::ClientSession *l_target = f_context.server()->clientById(l_uid);
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
        for (const QString &l_char_name : std::as_const(l_char_names)) {
            int char_id = f_context.server()->characterId(l_char_name);
            if (char_id == -1) {
                f_context.reply("Could not find character: " + l_char_name);
                return;
            }
            l_target->addCharCurse(char_id);
        }
    }

    l_target->setCharCursed(true);

    // The refresh shows the cursed player their narrowed character list; a
    // target caught outside the list goes back to the select screen.
    f_context.server()->updateCharsTaken(f_context.server()->areaById(f_context.areaId()));
    if (!l_target->charCurseList().contains(f_context.server()->characterId(l_target->character()))) {
        l_target->takeCharacter(-1, true);
    }

    l_target->sendServerMessage("You have been charcursed!");
    f_context.reply("Charcursed player.");
}

void cmdUnCharCurse(CommandContext &f_context)
{
    bool conv_ok = false;
    int l_uid = f_context.argument(0).toInt(&conv_ok);
    if (!conv_ok) {
        f_context.reply("Invalid user ID.");
        return;
    }

    akashi::ClientSession *l_target = f_context.server()->clientById(l_uid);
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

void cmdCharSelect(CommandContext &f_context)
{
    akashi::ClientSession *l_self = f_context.server()->clientById(f_context.clientId());
    l_self->takeCharacter(-1, true);
}

void cmdForceCharSelect(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        l_target->forceCharacterSelect();
        f_context.reply("Client has been forced into character select!");
    }
}

void cmdA(CommandContext &f_context)
{
    bool ok;
    int l_area_id = f_context.argument(0).toInt(&ok);
    if (!ok) {
        f_context.reply("This does not look like a valid AreaID.");
        return;
    }
    // areaById() returns nullptr for any id outside the range, so an
    // out-of-range (or negative) id would crash on the owners() call below.
    if (l_area_id < 0 || l_area_id >= f_context.server()->areaCount()) {
        f_context.reply("This does not look like a valid AreaID.");
        return;
    }

    akashi::Area *l_area = f_context.server()->areaById(l_area_id);
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

void cmdS(CommandContext &f_context)
{
    int l_all_areas = f_context.server()->areaCount() - 1;
    QString l_sender_name = f_context.name();
    QString l_ooc_message = f_context.arguments().join(" ");

    for (int i = 0; i <= l_all_areas; i++) {
        if (f_context.server()->areaById(i)->owners().contains(f_context.clientId()))
            f_context.server()->broadcast(akashi::Packet("CT", {"[CM]" + l_sender_name, l_ooc_message}), i);
    }
}

void cmdFirstPerson(CommandContext &f_context)
{
    akashi::ClientSession *l_self = f_context.server()->clientById(f_context.clientId());
    l_self->setFirstPerson(!l_self->isFirstPerson());
    QString l_str_en = l_self->isFirstPerson() ? "enabled" : "disabled";
    f_context.reply("First person mode " + l_str_en + ".");
}

void registerMessagingCommands(CommandRegistry &f_registry)
{
    f_registry.registerCommand({"pos", {}, {permission::user}, 1, "/pos <position>", "Moves you to a position in the area (def, pro, wit, jud, ...)."}, cmdPos, "core");
    f_registry.registerCommand({"forcepos", {}, {permission::gamemaster}, 2, "/forcepos <position> <id>", "Forces a client to a position."}, cmdForcePos, "core");
    f_registry.registerCommand({"g", {}, {permission::user}, 1, "/g <message>", "Sends a message to the global chat."}, cmdG, "core");
    f_registry.registerCommand({"need", {}, {permission::user}, 1, "/need <message>", "Sends a player advert to everyone with adverts enabled."}, cmdNeed, "core");
    f_registry.registerCommand({"switch", {}, {permission::user}, 1, "/switch <character>", "Switches you to the named character."}, cmdSwitch, "core");
    f_registry.registerCommand({"randomchar", {}, {permission::user}, 0, "/randomchar", "Picks a random character for you."}, cmdRandomChar, "core");
    f_registry.registerCommand({"toggleglobal", {}, {permission::user}, 0, "/toggleglobal", "Toggles whether you receive global chat."}, cmdToggleGlobal, "core");
    f_registry.registerCommand({"pm", {}, {permission::user}, 2, "/pm <id> <message>", "Sends a private message to a client."}, cmdPM, "core");
    f_registry.registerCommand({"announce", {}, {permission::announcer}, 1, "/announce <message>", "Sends an announcement to the whole server."}, cmdAnnounce, "core");
    f_registry.registerCommand({"m", {}, {permission::chat_moderator}, 1, "/m <message>", "Sends a message to the moderator chat."}, cmdM, "core");
    f_registry.registerCommand({"gm", {}, {permission::chat_moderator}, 1, "/gm <message>", "Sends a global message tagged as a moderator."}, cmdGM, "core");
    f_registry.registerCommand({"lm", {}, {permission::chat_moderator}, 1, "/lm <message>", "Sends an area message tagged as a moderator."}, cmdLM, "core");
    f_registry.registerCommand({"gimp", {}, {permission::mute}, 1, "/gimp <id> [until]", "Replaces a client's IC messages with canned lines, until a time like 1d12h if one is given."}, cmdGimp, "core");
    f_registry.registerCommand({"ungimp", {}, {permission::mute}, 1, "/ungimp <id>", "Lifts a client's gimp."}, cmdUngimp, "core");
    f_registry.registerCommand({"disemvowel", {}, {permission::mute}, 1, "/disemvowel <id> [until]", "Strips the vowels from a client's IC messages, until a time like 1d12h if one is given."}, cmdDisemvowel, "core");
    f_registry.registerCommand({"undisemvowel", {}, {permission::mute}, 1, "/undisemvowel <id>", "Gives a client their vowels back."}, cmdUnDisemvowel, "core");
    f_registry.registerCommand({"shake", {}, {permission::mute}, 1, "/shake <id> [until]", "Shuffles the words of a client's IC messages, until a time like 1d12h if one is given."}, cmdShake, "core");
    f_registry.registerCommand({"unshake", {}, {permission::mute}, 1, "/unshake <id>", "Stops shuffling a client's IC messages."}, cmdUnShake, "core");
    f_registry.registerCommand({"medieval", {}, {permission::mute}, 1, "/medieval <id> [until]", "Makes a client speak in medieval English, until a time like 1d12h if one is given."}, cmdMedieval, "core");
    f_registry.registerCommand({"unmedieval", {}, {permission::mute}, 1, "/unmedieval <id>", "Returns a client to plain speech."}, cmdUnmedieval, "core");
    f_registry.registerCommand({"medievalmode", {"medieval_mode"}, {permission::mute}, 0, "/medievalmode", "Toggles medieval mode in the area."}, cmdMedievalMode, "core");
    f_registry.registerCommand({"mutepm", {}, {permission::user}, 0, "/mutepm", "Toggles whether you receive private messages."}, cmdMutePM, "core");
    f_registry.registerCommand({"toggleadverts", {}, {permission::user}, 0, "/toggleadverts", "Toggles whether you receive player adverts."}, cmdToggleAdverts, "core");
    f_registry.registerCommand({"afk", {}, {permission::user}, 0, "/afk", "Marks you as away."}, cmdAfk, "core");
    f_registry.registerCommand({"charcurse", {}, {permission::mute}, 1, "/charcurse <id> [characters...]", "Restricts a client to the listed characters."}, cmdCharCurse, "core");
    f_registry.registerCommand({"uncharcurse", {}, {permission::mute}, 1, "/uncharcurse <id>", "Lifts a client's character restriction."}, cmdUnCharCurse, "core");
    f_registry.registerCommand({"charselect", {}, {permission::user}, 0, "/charselect [id]", "Returns you (or a target, with permission) to character select."}, cmdCharSelect, "core");
    f_registry.registerCommand({"force_charselect", {"forcecharselect"}, {permission::force_charselect}, 1, "/force_charselect <id>", "Forces a client back to character select."}, cmdForceCharSelect, "core");
    f_registry.registerCommand({"a", {}, {permission::user}, 2, "/a <area> <message>", "Sends a message to an area you own."}, cmdA, "core");
    f_registry.registerCommand({"s", {}, {permission::user}, 0, "/s <message>", "Sends a message to every area you own."}, cmdS, "core");
    f_registry.registerCommand({"firstperson", {}, {permission::user}, 0, "/firstperson", "Toggles first-person mode; your emotes stay hidden from others."}, cmdFirstPerson, "core");
}

} // namespace akashi::commands
