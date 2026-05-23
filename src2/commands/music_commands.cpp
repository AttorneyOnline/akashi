#include "commands/music_commands.h"

#include "akashi/permissions.h"
#include "aoclient.h"
#include "area_data.h"
#include "core/command_context.h"
#include "core/command_registry.h"
#include "core/command_spec.h"
#include "proto/packet.h"
#include "server.h"
#include "world/jukebox.h"

namespace akashi::commands {

static QString reprimand(Server *f_server, bool f_positive = false)
{
    if (f_positive)
        return f_server->praiseList().at(CommandContext::genRand(0, f_server->praiseList().size() - 1));
    else
        return f_server->reprimandsList().at(CommandContext::genRand(0, f_server->reprimandsList().size() - 1));
}

static void handlePlay(CommandContext &f_context)
{
    QString l_song = f_context.arguments().join(" ");
    AOClient *l_self = f_context.server()->clientById(f_context.clientId());
    if (l_self->hasSanction(akashi::sanction::dj_blocked)) {
        f_context.reply("You are blocked from changing the music.");
        return;
    }
    if (l_song == "sin.mp3") {
        l_self->closeSocket();
        return;
    }
    if (l_song.contains('/') && !akashi::Jukebox::validateSong(l_song, f_context.server()->cdnList())) {
        f_context.reply("The song you tried to play is not from an approved CDN.");
        return;
    }
    AreaData *l_area = f_context.server()->areaById(f_context.areaId());
    if (!l_area->owners().contains(f_context.clientId()) && !l_area->isPlayEnabled() && !f_context.canPerform(akashi::permission::gamemaster)) {
        f_context.reply("Free music play is disabled in this area.");
        return;
    }
    if (f_context.characterName().isEmpty()) {
        l_area->changeMusic(f_context.character(), l_song);
    }
    else {
        l_area->changeMusic(f_context.characterName(), l_song);
    }
    akashi::Packet music_change("MC", {l_song, QString::number(f_context.server()->characterId(f_context.character())), f_context.characterName(), "1", "0"});
    f_context.server()->broadcast(music_change, f_context.areaId());
}

static void handlePlayAmbience(CommandContext &f_context)
{
    AOClient *l_self = f_context.server()->clientById(f_context.clientId());
    if (l_self->hasSanction(akashi::sanction::dj_blocked)) {
        f_context.reply("You are blocked from changing the ambience.");
        return;
    }
    AreaData *l_area = f_context.server()->areaById(f_context.areaId());
    if (!l_area->owners().contains(f_context.clientId()) && !l_area->isPlayEnabled()) {
        f_context.reply("Free ambience play is disabled in this area.");
        return;
    }
    QString l_song = f_context.arguments().join(" ");
    if (l_song.contains('/') && !akashi::Jukebox::validateSong(l_song, f_context.server()->cdnList())) {
        f_context.reply("The song you tried to play is not from an approved CDN.");
        return;
    }
    l_area->changeAmbience(l_song);
    akashi::Packet music_change("MC", {l_song, "-1", f_context.characterName(), "1", "1"});
    f_context.server()->broadcast(music_change, f_context.areaId());
}

static void handleCurrentMusic(CommandContext &f_context)
{
    AreaData *l_area = f_context.server()->areaById(f_context.areaId());
    if (!l_area->currentMusic().isEmpty() && !l_area->currentMusic().contains("~stop.mp3"))
        f_context.reply("The current song is " + l_area->currentMusic() + " played by " + l_area->musicPlayerBy());
    else
        f_context.reply("There is no music playing.");
}

static void handleBlockDj(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        if (l_target->hasSanction(akashi::sanction::dj_blocked))
            f_context.reply("That player is already DJ blocked!");
        else {
            f_context.reply("DJ blocked player.");
            l_target->reply("You were blocked from changing the music by a moderator. " + reprimand(f_context.server()));
        }
        l_target->setSanction(akashi::sanction::dj_blocked, true);
    }
}

static void handleUnblockDj(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        if (!l_target->hasSanction(akashi::sanction::dj_blocked))
            f_context.reply("That player is not DJ blocked!");
        else {
            f_context.reply("DJ permissions restored to player.");
            l_target->reply("A moderator restored your music permissions. " + reprimand(f_context.server(), true));
        }
        l_target->setSanction(akashi::sanction::dj_blocked, false);
    }
}

static void handleToggleMusic(CommandContext &f_context)
{
    AreaData *l_area = f_context.server()->areaById(f_context.areaId());
    l_area->toggleMusic();
    QString l_state = l_area->isMusicAllowed() ? "allowed." : "disallowed.";
    f_context.reply("Music in this area is now " + l_state);
}

static void handleToggleJukebox(CommandContext &f_context)
{
    AreaData *l_area = f_context.server()->areaById(f_context.areaId());
    l_area->toggleJukebox();
    QString l_state = l_area->isJukeboxEnabled() ? "enabled." : "disabled.";
    f_context.replyToArea("The jukebox in this area has been " + l_state);
}

static void handleAddMusic(CommandContext &f_context)
{
    QString l_argv_string = f_context.arguments().join(" ");
    QStringList l_argv = l_argv_string.split(",");

    if (l_argv.size() >= 4) {
        f_context.reply("Too many arguments. Addition of song has failed.");
        return;
    }

    QString l_song_name = l_argv.value(0).trimmed();
    QString l_true_name = l_argv.size() >= 2 ? l_argv.value(1).trimmed() : l_song_name;
    int l_song_duration = 0;
    if (l_argv.size() >= 3) {
        bool ok;
        l_song_duration = l_argv.value(2).trimmed().toInt(&ok);
        if (!ok)
            l_song_duration = 0;
    }

    akashi::Jukebox *l_jukebox = f_context.server()->areaById(f_context.areaId())->jukebox();
    f_context.reply(l_jukebox->addSong({l_song_name, l_true_name, l_song_duration}));
}

static void handleAddMusicCategory(CommandContext &f_context)
{
    akashi::Jukebox *l_jukebox = f_context.server()->areaById(f_context.areaId())->jukebox();
    f_context.reply(l_jukebox->addCategory(f_context.arguments().join(" ")));
}

static void handleRemoveCustomMusic(CommandContext &f_context)
{
    akashi::Jukebox *l_jukebox = f_context.server()->areaById(f_context.areaId())->jukebox();
    bool l_success = l_jukebox->removeSong(f_context.arguments().join(" "));
    QString l_message = l_success ? "succeeded." : "failed.";
    f_context.reply("The removal of the entry has " + l_message);
}

static void handleToggleCustomMusic(CommandContext &f_context)
{
    akashi::Jukebox *l_jukebox = f_context.server()->areaById(f_context.areaId())->jukebox();
    l_jukebox->resetToFloor();
    f_context.reply("Custom music list has been reset to the floor defaults.");
}

static void handleClearCustomMusic(CommandContext &f_context)
{
    f_context.server()->areaById(f_context.areaId())->jukebox()->resetToFloor();
    f_context.reply("Custom songs have been cleared.");
}

static void handleJukeboxSkip(CommandContext &f_context)
{
    QString l_name = f_context.character();
    if (!f_context.characterName().isEmpty()) {
        l_name = f_context.characterName();
    }

    AreaData *l_area = f_context.server()->areaById(f_context.areaId());

    if (l_area->isJukeboxEnabled()) {
        if (l_area->jukebox()->skip()) {
            f_context.replyToArea(l_name + " has forced a skip. Playing the next available song.");
            return;
        }
        f_context.reply("Unable to skip song. Jukebox is currently empty.");
        return;
    }
    f_context.reply("Unable to skip song. The jukebox is not running.");
}

void registerMusicCommands(CommandRegistry &f_registry)
{
    f_registry.registerCommand(
        {QStringLiteral("play"), {}, {}, 1,
         QStringLiteral("/play <song>"),
         QStringLiteral("Plays a song in the area.")},
        handlePlay, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("play_ambience"), {QStringLiteral("playambience"), QStringLiteral("playa")}, {}, 1,
         QStringLiteral("/play_ambience <song>"),
         QStringLiteral("Plays ambient music in the area.")},
        handlePlayAmbience, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("currentmusic"), {}, {}, 0,
         QStringLiteral("/currentmusic"),
         QStringLiteral("Shows the currently playing song in the area.")},
        handleCurrentMusic, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("block_dj"), {QStringLiteral("blockdj")}, {QStringLiteral("mute")}, 1,
         QStringLiteral("/block_dj <id>"),
         QStringLiteral("Blocks a client from changing music.")},
        handleBlockDj, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("unblock_dj"), {QStringLiteral("unblockdj")}, {QStringLiteral("mute")}, 1,
         QStringLiteral("/unblock_dj <id>"),
         QStringLiteral("Restores a client's music permissions.")},
        handleUnblockDj, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("togglemusic"), {}, {QStringLiteral("gamemaster")}, 0,
         QStringLiteral("/togglemusic"),
         QStringLiteral("Toggles music playing in the area.")},
        handleToggleMusic, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("togglejukebox"), {}, {QStringLiteral("gamemaster"), QStringLiteral("jukebox")}, 0,
         QStringLiteral("/togglejukebox"),
         QStringLiteral("Toggles the jukebox in the area.")},
        handleToggleJukebox, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("addmusic"), {}, {QStringLiteral("gamemaster")}, 1,
         QStringLiteral("/addmusic <name>[, truename][, duration]"),
         QStringLiteral("Adds a song to the custom music list.")},
        handleAddMusic, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("addmusiccategory"), {}, {QStringLiteral("gamemaster")}, 1,
         QStringLiteral("/addmusiccategory <name>"),
         QStringLiteral("Adds a category to the custom music list.")},
        handleAddMusicCategory, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("removecustommusic"), {}, {QStringLiteral("gamemaster")}, 1,
         QStringLiteral("/removecustommusic <name>"),
         QStringLiteral("Removes a song or category from the custom list.")},
        handleRemoveCustomMusic, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("togglecustommusic"), {}, {QStringLiteral("gamemaster")}, 0,
         QStringLiteral("/togglecustommusic"),
         QStringLiteral("Resets the custom music list to floor defaults.")},
        handleToggleCustomMusic, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("clearcustommusic"), {}, {QStringLiteral("gamemaster")}, 0,
         QStringLiteral("/clearcustommusic"),
         QStringLiteral("Clears all custom songs in the area.")},
        handleClearCustomMusic, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("jukebox_skip"), {QStringLiteral("jukeboxskip")}, {QStringLiteral("gamemaster")}, 0,
         QStringLiteral("/jukebox_skip"),
         QStringLiteral("Skips the current jukebox song.")},
        handleJukeboxSkip, QStringLiteral("core"));
}

} // namespace akashi::commands
