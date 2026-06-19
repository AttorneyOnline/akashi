#pragma once

#include "akashi/jukebox_policy.h"
#include "akashi_core_export.h"

#include <QMap>
#include <QObject>
#include <QStringList>
#include <QUrl>

#include <memory>
#include <optional>

class QTimer;

namespace akashi {

class Floor;

// The music system of one area: catalog + playback in one place. The
// catalog is two tiers - the floor's music list (inherited, shared) and
// area-level custom overrides layered on top. Playback delegates every
// decision to a JukeboxPolicy; core installs the queue basis mode.
//
// The jukebox never touches the wire: it emits semantic signals and the
// server builds FM/MC packets from them.
class AKASHI_CORE_EXPORT Jukebox : public QObject
{
    Q_OBJECT

  public:
    explicit Jukebox(QObject *parent = nullptr);
    ~Jukebox() override;

    // ── Floor catalog (inherited baseline) ───────────────────────
    // Called once when the area is set up. The jukebox reads from the
    // floor's data; it never modifies it.
    void setFloorCatalog(const Floor *f_floor);

    // ── Custom catalog (per-area overrides) ──────────────────────
    // Same-name adds overwrite the floor entry's metadata. Returns a
    // user-facing result message.
    QString addSong(const JukeboxSong &f_song);
    QString addCategory(const QString &f_name);
    bool removeSong(const QString &f_name);
    void resetToFloor();

    // ── Catalog queries ──────────────────────────────────────────
    // The resolved list merges floor + customs for FM: floor entries
    // first (with custom metadata where overridden), then custom-only
    // entries appended at the end.
    QStringList resolvedList() const;
    std::optional<JukeboxSong> songInfo(const QString &f_name) const;
    bool hasSong(const QString &f_name) const;

    // Songs in one category (between two == markers), for shuffle
    // policies. All playable songs across every category.
    QList<JukeboxSong> songsInCategory(const QString &f_category) const;
    QList<JukeboxSong> allPlayableSongs() const;

    // ── Song validation ──────────────────────────────────────────
    static bool validateSong(const QString &f_name, const QStringList &f_cdns);

    // ── Music state ───────────────────
    QString currentSong() const;
    QString currentAmbience() const;
    QString musicPlayedBy() const;

    void changeMusic(const QString &f_song, const QString &f_played_by);
    void changeAmbience(const QString &f_song);

    // ── Playback (existing jukebox engine) ───────────────────────
    bool isPlaying() const;
    QString currentSongName() const;
    int pendingCount() const;

    void setPolicy(std::unique_ptr<JukeboxPolicy> f_policy);
    QString request(int f_player_id, const JukeboxSong &f_song);
    void playerLeft(int f_player_id);
    bool skip();
    void start();
    void stop();

    // Resolves a song name through the catalog and queues it. Returns
    // the policy's answer message.
    QString queueSong(int f_player_id, const QString &f_name);

  Q_SIGNALS:
    void songStarted(const akashi::JukeboxSong &f_song);
    void ambienceChanged(const QString &f_song);
    void musicListChanged();

  private:
    bool playNext();

    // Floor catalog (not owned)
    const Floor *m_floor = nullptr;

    // Per-area custom overrides
    QStringList m_custom_ordered;
    QMap<QString, JukeboxSong> m_custom_songs;

    // Music state
    QString m_current_song;
    QString m_current_ambience;
    QString m_played_by;

    // Playback engine
    QTimer *m_timer;
    std::unique_ptr<JukeboxPolicy> m_policy;
    JukeboxSong m_current;
};

} // namespace akashi

