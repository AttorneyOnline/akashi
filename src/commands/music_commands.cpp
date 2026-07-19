#include "commands/music_commands.h"

#include "akashi/permissions.h"
#include "commands/moderation_commands.h"
#include "core/client_session.h"
#include "core/command_context.h"
#include "core/command_registry.h"
#include "core/command_spec.h"
#include "core/server_context.h"
#include "world/area.h"
#include "world/jukebox.h"

namespace akashi::commands {

static QString reprimand(ServerContext *f_server, bool f_positive = false)
{
    if (f_positive)
        return f_server->praiseList().at(CommandContext::genRand(0, f_server->praiseList().size() - 1));
    else
        return f_server->reprimandsList().at(CommandContext::genRand(0, f_server->reprimandsList().size() - 1));
}

void cmdPlay(CommandContext &f_context)
{
    QString l_song = f_context.arguments().join(" ");
    akashi::ClientSession *l_self = f_context.server()->clientById(f_context.clientId());
    if (l_self->isDjBlocked()) {
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
    // Free play is gated by the check_free_play floor rule.
    if (auto l_refusal = l_self->playMusic(l_song, akashi::MusicSource::PlayCommand, {}, {})) {
        f_context.reply(*l_refusal);
    }
}

void cmdPlayAmbience(CommandContext &f_context)
{
    akashi::ClientSession *l_self = f_context.server()->clientById(f_context.clientId());
    if (l_self->isDjBlocked()) {
        f_context.reply("You are blocked from changing the ambience.");
        return;
    }
    QString l_song = f_context.arguments().join(" ");
    if (l_song.contains('/') && !akashi::Jukebox::validateSong(l_song, f_context.server()->cdnList())) {
        f_context.reply("The song you tried to play is not from an approved CDN.");
        return;
    }
    // Free play is gated by the check_free_play floor rule.
    if (auto l_refusal = l_self->playAmbience(l_song, akashi::MusicSource::PlayCommand)) {
        f_context.reply(*l_refusal);
    }
}

void cmdCurrentMusic(CommandContext &f_context)
{
    akashi::Area *l_area = f_context.server()->areaById(f_context.areaId());
    if (!l_area->currentMusic().isEmpty() && !l_area->currentMusic().contains("~stop.mp3"))
        f_context.reply("The current song is " + l_area->currentMusic() + " played by " + l_area->musicPlayerBy());
    else
        f_context.reply("There is no music playing.");
}

void cmdBlockDj(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        const bool l_was_sanctioned = l_target->hasSanction(akashi::sanction::dj_blocked);
        if (!applySanction(f_context, *l_target, akashi::sanction::dj_blocked))
            return;
        if (l_was_sanctioned)
            f_context.reply("That player is already DJ blocked!");
        else {
            f_context.reply("DJ blocked player.");
            l_target->reply("You were blocked from changing the music by a moderator. " + reprimand(f_context.server()));
        }
    }
}

void cmdUnblockDj(CommandContext &f_context)
{
    if (auto l_target = f_context.resolveTarget()) {
        const bool l_was_sanctioned = l_target->hasSanction(akashi::sanction::dj_blocked);
        liftSanction(f_context, *l_target, akashi::sanction::dj_blocked);
        if (!l_was_sanctioned)
            f_context.reply("That player is not DJ blocked!");
        else {
            f_context.reply("DJ permissions restored to player.");
            l_target->reply("A moderator restored your music permissions. " + reprimand(f_context.server(), true));
        }
    }
}

void cmdToggleMusic(CommandContext &f_context)
{
    akashi::Area *l_area = f_context.server()->areaById(f_context.areaId());
    l_area->toggleMusic();
    QString l_state = l_area->isMusicAllowed() ? "allowed." : "disallowed.";
    f_context.reply("Music in this area is now " + l_state);
}

void cmdToggleJukebox(CommandContext &f_context)
{
    akashi::Area *l_area = f_context.server()->areaById(f_context.areaId());
    l_area->toggleJukebox();
    QString l_state = l_area->isJukeboxEnabled() ? "enabled." : "disabled.";
    f_context.replyToArea("The jukebox in this area has been " + l_state);
}

void cmdAddMusic(CommandContext &f_context)
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

void cmdAddMusicCategory(CommandContext &f_context)
{
    akashi::Jukebox *l_jukebox = f_context.server()->areaById(f_context.areaId())->jukebox();
    f_context.reply(l_jukebox->addCategory(f_context.arguments().join(" ")));
}

void cmdRemoveCustomMusic(CommandContext &f_context)
{
    akashi::Jukebox *l_jukebox = f_context.server()->areaById(f_context.areaId())->jukebox();
    bool l_success = l_jukebox->removeSong(f_context.arguments().join(" "));
    QString l_message = l_success ? "succeeded." : "failed.";
    f_context.reply("The removal of the entry has " + l_message);
}

void cmdToggleCustomMusic(CommandContext &f_context)
{
    akashi::Jukebox *l_jukebox = f_context.server()->areaById(f_context.areaId())->jukebox();
    l_jukebox->resetToFloor();
    f_context.reply("Custom music list has been reset to the floor defaults.");
}

void cmdClearCustomMusic(CommandContext &f_context)
{
    f_context.server()->areaById(f_context.areaId())->jukebox()->resetToFloor();
    f_context.reply("Custom songs have been cleared.");
}

void cmdJukeboxSkip(CommandContext &f_context)
{
    QString l_name = f_context.character();
    if (!f_context.characterName().isEmpty()) {
        l_name = f_context.characterName();
    }

    akashi::Area *l_area = f_context.server()->areaById(f_context.areaId());

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
        {QStringLiteral("play"), {}, {akashi::permission::music_play}, 1, QStringLiteral("/play <song>"), QStringLiteral("Plays a song in the area.")},
        cmdPlay, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("play_ambience"), {QStringLiteral("playambience"), QStringLiteral("playa")}, {akashi::permission::music_play_ambience}, 1, QStringLiteral("/play_ambience <song>"), QStringLiteral("Plays ambient music in the area.")},
        cmdPlayAmbience, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("currentmusic"), {}, {akashi::permission::music_currentmusic}, 0, QStringLiteral("/currentmusic"), QStringLiteral("Shows the currently playing song in the area.")},
        cmdCurrentMusic, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("block_dj"), {QStringLiteral("blockdj")}, {akashi::permission::sanction_block_dj}, 1, QStringLiteral("/block_dj <id> [until]"), QStringLiteral("Blocks a client from changing music, until a time like 1d12h if one is given.")},
        cmdBlockDj, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("unblock_dj"), {QStringLiteral("unblockdj")}, {akashi::permission::sanction_block_dj}, 1, QStringLiteral("/unblock_dj <id>"), QStringLiteral("Restores a client's music permissions.")},
        cmdUnblockDj, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("togglemusic"), {}, {akashi::permission::cm_togglemusic}, 0, QStringLiteral("/togglemusic"), QStringLiteral("Toggles music playing in the area.")},
        cmdToggleMusic, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("togglejukebox"), {}, {akashi::permission::music_jukebox}, 0, QStringLiteral("/togglejukebox"), QStringLiteral("Toggles the jukebox in the area.")},
        cmdToggleJukebox, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("addmusic"), {}, {akashi::permission::cm_addmusic}, 1, QStringLiteral("/addmusic <name>[, truename][, duration]"), QStringLiteral("Adds a song to the custom music list.")},
        cmdAddMusic, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("addmusiccategory"), {}, {akashi::permission::cm_addmusiccategory}, 1, QStringLiteral("/addmusiccategory <name>"), QStringLiteral("Adds a category to the custom music list.")},
        cmdAddMusicCategory, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("removecustommusic"), {}, {akashi::permission::cm_removecustommusic}, 1, QStringLiteral("/removecustommusic <name>"), QStringLiteral("Removes a song or category from the custom list.")},
        cmdRemoveCustomMusic, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("togglecustommusic"), {}, {akashi::permission::cm_togglecustommusic}, 0, QStringLiteral("/togglecustommusic"), QStringLiteral("Resets the custom music list to floor defaults.")},
        cmdToggleCustomMusic, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("clearcustommusic"), {}, {akashi::permission::cm_clearcustommusic}, 0, QStringLiteral("/clearcustommusic"), QStringLiteral("Clears all custom songs in the area.")},
        cmdClearCustomMusic, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("jukebox_skip"), {QStringLiteral("jukeboxskip")}, {akashi::permission::cm_jukebox_skip}, 0, QStringLiteral("/jukebox_skip"), QStringLiteral("Skips the current jukebox song.")},
        cmdJukeboxSkip, QStringLiteral("core"));
}

} // namespace akashi::commands
