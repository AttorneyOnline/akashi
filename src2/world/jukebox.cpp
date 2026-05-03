#include "world/jukebox.h"

#include "world/floor.h"

#include <QRandomGenerator>
#include <QTimer>
#include <QUrl>
#include <QVector>

namespace akashi {

namespace {

// The basis mode: requested songs play once each in random order, and the
// last one keeps looping until new requests arrive - the room never falls
// silent once something played.
class QueuePolicy : public JukeboxPolicy
{
  public:
    QString onRequest(int f_player_id, const JukeboxSong &f_song) override
    {
        Q_UNUSED(f_player_id)
        if (f_song.seconds <= 0) {
            return QStringLiteral("Unable to add song. Duration shorter than 1.");
        }
        for (const JukeboxSong &l_queued : m_queue) {
            if (l_queued.name == f_song.name) {
                return QStringLiteral("Unable to add song. Song already in Jukebox.");
            }
        }
        m_queue.append(f_song);
        return QStringLiteral("Song added to Jukebox.");
    }

    void onPlayerLeft(int f_player_id) override
    {
        Q_UNUSED(f_player_id)
    }

    std::optional<JukeboxSong> pickNext() override
    {
        if (m_queue.isEmpty()) {
            return std::nullopt;
        }
        if (m_queue.size() == 1) {
            return m_queue.first();
        }
        return m_queue.takeAt(QRandomGenerator::global()->bounded(m_queue.size()));
    }

    void reset() override
    {
        m_queue.clear();
    }

    int pendingCount() const override
    {
        return m_queue.size();
    }

  private:
    QVector<JukeboxSong> m_queue;
};

static const QStringList ALLOWED_EXTENSIONS = {".opus", ".ogg", ".mp3", ".wav"};

} // namespace

Jukebox::Jukebox(QObject *parent) :
    QObject(parent),
    m_timer(new QTimer(this)),
    m_policy(std::make_unique<QueuePolicy>())
{
    connect(m_timer, &QTimer::timeout, this, &Jukebox::playNext);
}

Jukebox::~Jukebox() = default;

// ── Floor catalog ────────────────────────────────────────────────

void Jukebox::setFloorCatalog(const Floor *f_floor)
{
    m_floor = f_floor;
    m_custom_ordered.clear();
    m_custom_songs.clear();
    Q_EMIT musicListChanged();
}

// ── Custom catalog ───────────────────────────────────────────────

QString Jukebox::addSong(const JukeboxSong &f_song)
{
    if (f_song.name.isEmpty()) {
        return QStringLiteral("Song name cannot be empty.");
    }

    if (m_floor) {
        const QStringList &l_cdns = m_floor->approved_cdns;
        if (!l_cdns.isEmpty()) {
            if (!validateSong(f_song.name, l_cdns) || !validateSong(f_song.real_name, l_cdns)) {
                return QStringLiteral("The song is not from an approved source.");
            }
        }
    }

    // Same-name adds overwrite - this is the deliberate change from the
    // old MusicManager which rejected duplicates.
    if (!m_custom_ordered.contains(f_song.name)) {
        m_custom_ordered.append(f_song.name);
    }
    m_custom_songs.insert(f_song.name, f_song);
    Q_EMIT musicListChanged();
    return QStringLiteral("Song added successfully.");
}

QString Jukebox::addCategory(const QString &f_name)
{
    if (f_name.isEmpty()) {
        return QStringLiteral("Category name cannot be empty.");
    }
    if (f_name.contains('.')) {
        return QStringLiteral("Category names cannot contain a file extension.");
    }

    QString l_name = f_name;
    if (!l_name.startsWith("==")) {
        l_name = "==" + l_name + "==";
    }

    if (!m_custom_ordered.contains(l_name)) {
        m_custom_ordered.append(l_name);
    }
    m_custom_songs.insert(l_name, {l_name, l_name, 0});
    Q_EMIT musicListChanged();
    return QStringLiteral("Category added successfully.");
}

bool Jukebox::removeSong(const QString &f_name)
{
    if (!m_custom_songs.contains(f_name)) {
        return false;
    }
    m_custom_songs.remove(f_name);
    m_custom_ordered.removeAll(f_name);
    Q_EMIT musicListChanged();
    return true;
}

void Jukebox::resetToFloor()
{
    m_custom_ordered.clear();
    m_custom_songs.clear();
    Q_EMIT musicListChanged();
}

// ── Catalog queries ──────────────────────────────────────────────

QStringList Jukebox::resolvedList() const
{
    QStringList l_result;

    // Floor entries first, using custom metadata where overridden.
    if (m_floor) {
        l_result = m_floor->music_ordered;
    }

    // Append custom-only entries (names not already in the floor list).
    for (const QString &l_name : m_custom_ordered) {
        if (!l_result.contains(l_name)) {
            l_result.append(l_name);
        }
    }

    return l_result;
}

std::optional<JukeboxSong> Jukebox::songInfo(const QString &f_name) const
{
    // Custom overrides take precedence.
    auto l_custom = m_custom_songs.find(f_name);
    if (l_custom != m_custom_songs.end()) {
        return *l_custom;
    }
    if (m_floor) {
        auto l_floor = m_floor->music_songs.find(f_name);
        if (l_floor != m_floor->music_songs.end()) {
            return *l_floor;
        }
    }
    return std::nullopt;
}

bool Jukebox::hasSong(const QString &f_name) const
{
    return m_custom_songs.contains(f_name) || (m_floor && m_floor->music_songs.contains(f_name));
}

QList<JukeboxSong> Jukebox::songsInCategory(const QString &f_category) const
{
    QList<JukeboxSong> l_songs;
    const QStringList l_list = resolvedList();
    bool l_in_category = false;

    for (const QString &l_name : l_list) {
        if (l_name.startsWith("==")) {
            l_in_category = (l_name == f_category);
            continue;
        }
        if (!l_in_category) {
            continue;
        }
        const std::optional<JukeboxSong> l_song = songInfo(l_name);
        if (l_song && l_song->seconds > 0) {
            l_songs.append(*l_song);
        }
    }
    return l_songs;
}

QList<JukeboxSong> Jukebox::allPlayableSongs() const
{
    QList<JukeboxSong> l_songs;
    const QStringList l_list = resolvedList();
    for (const QString &l_name : l_list) {
        if (l_name.startsWith("==")) {
            continue;
        }
        const std::optional<JukeboxSong> l_song = songInfo(l_name);
        if (l_song && l_song->seconds > 0) {
            l_songs.append(*l_song);
        }
    }
    return l_songs;
}

// ── Validation ───────────────────────────────────────────────────

bool Jukebox::validateSong(const QString &f_name, const QStringList &f_cdns)
{
    QString l_path = f_name;

    if (f_name.contains('/')) {
        const QUrl l_url(f_name);
        if (l_url.scheme() != "https" && l_url.scheme() != "http") {
            return false;
        }

        bool l_approved = false;
        for (const QString &l_cdn : f_cdns) {
            const QString l_domain = QUrl::fromUserInput(l_cdn.trimmed()).host();
            if (!l_domain.isEmpty() && l_url.host().compare(l_domain, Qt::CaseInsensitive) == 0) {
                l_approved = true;
                break;
            }
        }
        if (!l_approved) {
            return false;
        }

        l_path = l_url.path();
    }

    for (const QString &l_ext : ALLOWED_EXTENSIONS) {
        if (l_path.endsWith(l_ext, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

// ── Music state ──────────────────────────────────────────────────

QString Jukebox::currentSong() const
{
    return m_current_song;
}

QString Jukebox::currentAmbience() const
{
    return m_current_ambience;
}

QString Jukebox::musicPlayedBy() const
{
    return m_played_by;
}

void Jukebox::changeMusic(const QString &f_song, const QString &f_played_by)
{
    m_current_song = f_song;
    m_played_by = f_played_by;
}

void Jukebox::changeAmbience(const QString &f_song)
{
    m_current_ambience = f_song;
    Q_EMIT ambienceChanged(f_song);
}

// ── Playback engine ──────────────────────────────────────────────

bool Jukebox::isPlaying() const
{
    return m_timer->isActive();
}

QString Jukebox::currentSongName() const
{
    return m_current.name;
}

int Jukebox::pendingCount() const
{
    return m_policy->pendingCount();
}

void Jukebox::setPolicy(std::unique_ptr<JukeboxPolicy> f_policy)
{
    if (f_policy) {
        m_policy = std::move(f_policy);
    }
}

QString Jukebox::request(int f_player_id, const JukeboxSong &f_song)
{
    const QString l_answer = m_policy->onRequest(f_player_id, f_song);
    if (!m_timer->isActive()) {
        playNext();
    }
    return l_answer;
}

void Jukebox::playerLeft(int f_player_id)
{
    m_policy->onPlayerLeft(f_player_id);
}

bool Jukebox::skip()
{
    return playNext();
}

void Jukebox::start()
{
    if (!m_timer->isActive()) {
        playNext();
    }
}

void Jukebox::stop()
{
    m_timer->stop();
    m_current = {};
    m_policy->reset();
}

QString Jukebox::queueSong(int f_player_id, const QString &f_name)
{
    const std::optional<JukeboxSong> l_info = songInfo(f_name);
    if (!l_info) {
        return QStringLiteral("Song not found in the music list.");
    }
    return request(f_player_id, *l_info);
}

bool Jukebox::playNext()
{
    const std::optional<JukeboxSong> l_pick = m_policy->pickNext();
    if (!l_pick) {
        m_timer->stop();
        m_current = {};
        return false;
    }
    m_current = *l_pick;
    Q_EMIT songStarted(m_current);
    m_timer->start(m_current.seconds * 1000);
    return true;
}

} // namespace akashi
