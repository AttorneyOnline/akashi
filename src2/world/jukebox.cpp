#include "world/jukebox.h"

#include <QRandomGenerator>
#include <QTimer>
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

} // namespace

Jukebox::Jukebox(QObject *parent) :
    QObject(parent),
    m_timer(new QTimer(this)),
    m_policy(std::make_unique<QueuePolicy>())
{
    connect(m_timer, &QTimer::timeout, this, &Jukebox::playNext);
}

Jukebox::~Jukebox() = default;

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
