#pragma once

#include <QString>

#include <optional>

namespace akashi {

// One song a jukebox can play: the name players see in the music list,
// the real file or stream the client loads, and the playtime that decides
// when the jukebox moves on.
struct JukeboxSong
{
    QString name;
    QString real_name;
    int seconds = 0;
};

// The decision seat of an area's jukebox, shippable by a plugin. The
// jukebox owns WHEN a song ends; its policy owns WHAT plays next and what
// a player's song request means - a queue spot, a vote, a refusal. Core
// ships the queue policy; richer regimes like vote-weighted picks (where
// a player's vote leaves the pool with them) or category and whole-list
// shuffles are policies a plugin installs once the plugin system lands.
//
// The ABI stays stable across releases under the same ground rules as the
// area rule contract: JukeboxSong only ever gains new fields at the end,
// and new capabilities arrive as new virtual methods added at the end -
// existing methods are never changed, reordered or removed.
class JukeboxPolicy
{
  public:
    virtual ~JukeboxPolicy() = default;

    // A player asks for a song; the returned text answers them.
    virtual QString onRequest(int f_player_id, const JukeboxSong &f_song) = 0;

    // A player left the area, taking their influence with them.
    virtual void onPlayerLeft(int f_player_id) = 0;

    // The next song to play; nothing means the jukebox falls silent.
    virtual std::optional<JukeboxSong> pickNext() = 0;

    // The jukebox was switched off; forget everything pending.
    virtual void reset() = 0;

    // How many requests or votes wait on a pick, for status messages.
    virtual int pendingCount() const = 0;
};

} // namespace akashi
