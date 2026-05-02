#ifndef WORLD_JUKEBOX_H
#define WORLD_JUKEBOX_H

#include "akashi/jukebox_policy.h"
#include "akashi_core_export.h"

#include <QObject>

#include <memory>

class QTimer;

namespace akashi {

// The jukebox of one area. It runs the clock - a song plays for its
// duration, then the next pick starts - and leaves every decision about
// what plays next to its policy; core installs the queue policy, a plugin
// can install its own. It knows nothing about packets or the music list:
// whoever owns it feeds resolved songs in and broadcasts what songStarted
// hands out.
class AKASHI_CORE_EXPORT Jukebox : public QObject
{
    Q_OBJECT

  public:
    explicit Jukebox(QObject *parent = nullptr);
    ~Jukebox() override;

    bool isPlaying() const;
    QString currentSongName() const;
    int pendingCount() const;

    // Replaces the decision policy. The old one's pending state goes with
    // it; the playing song finishes and the next pick asks the new policy.
    void setPolicy(std::unique_ptr<JukeboxPolicy> f_policy);

    // A player's song request, answered by the policy. An idle jukebox
    // starts playing right away.
    QString request(int f_player_id, const JukeboxSong &f_song);

    // A leaving player takes their influence (like a vote) with them.
    void playerLeft(int f_player_id);

    // Plays the next pick now; false when the policy has nothing.
    bool skip();

    // Starts playback when idle and the policy has something to play;
    // used when the jukebox is switched back on.
    void start();

    // Stops playback and forgets everything pending; keeps the policy.
    void stop();

  Q_SIGNALS:
    void songStarted(const akashi::JukeboxSong &f_song);

  private:
    bool playNext();

    QTimer *m_timer;
    std::unique_ptr<JukeboxPolicy> m_policy;
    JukeboxSong m_current;
};

} // namespace akashi

#endif // WORLD_JUKEBOX_H
