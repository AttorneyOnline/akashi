#pragma once

#include "akashi_core_export.h"

#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

namespace akashi {

// The core state of one area: identity, its place on the map, who is in it,
// and the moderation state the area list shows. Every mutation goes through
// a setter that reports actual changes, so the area-update broadcaster can
// subscribe instead of being called by hand at every site.
//
// The map is a grid: y is the floor (hub), x the area's position on it -
// the same coordinates the GA packet moves by. Evidence, testimony and the
// jukebox are separate pieces, not part of this core.
class AKASHI_CORE_EXPORT Area : public QObject
{
    Q_OBJECT

  public:
    enum class LockState
    {
        Free,
        Spectatable,
        Locked,
    };
    Q_ENUM(LockState)

    Area(int f_id, const QString &f_name, int f_floor_id, int f_x, QObject *parent = nullptr);

    // Identity and place on the map.
    int id() const { return m_id; }
    QString name() const { return m_name; }
    void setName(const QString &f_name) { m_name = f_name; }
    int floorId() const { return m_floor_id; }
    int x() const { return m_x; }

    // Removing an area compacts the id space; the server renumbers the
    // surviving areas with this.
    void renumber(int f_id, int f_floor_id, int f_x)
    {
        m_id = f_id;
        m_floor_id = f_floor_id;
        m_x = f_x;
    }

    // Which areas a player standing here has in their area list, so big maps
    // don't show hundreds of entries. Empty means every area on the floor -
    // the default, and today's behavior.
    QSet<int> visibleAreas() const { return m_visible_areas; }
    void setVisibleAreas(const QSet<int> &f_area_ids) { m_visible_areas = f_area_ids; }

    // Membership, keyed by player id: each character somebody plays is its
    // own entry here.
    QVector<int> players() const { return m_players; }
    int playerCount() const { return m_players.size(); }
    void addPlayer(int f_player_id);
    void removePlayer(int f_player_id);

    // The characters in use here, in the order they were taken. Whether
    // taking one is allowed is decided by the floor's character policy,
    // checked at the world level - this only tracks this area's own use.
    QList<int> charactersTaken() const { return m_characters_taken; }
    bool takeCharacter(int f_char_id);
    void releaseCharacter(int f_char_id);

    // Case managers and invitations.
    QList<int> owners() const { return m_owners; }
    void addOwner(int f_player_id);
    bool removeOwner(int f_player_id);
    bool hasOwners() const { return !m_owners.isEmpty(); }
    QList<int> invited() const { return m_invited; }
    bool invite(int f_player_id);
    bool uninvite(int f_player_id);

    LockState lockState() const { return m_lock_state; }
    void setLockState(LockState f_state);

    // The status line the area list shows. The protocol never limited it to
    // fixed values - the well-known ones (IDLE, CASING, ...) are convention -
    // so any text works; anything past 30 characters is cut off.
    QString status() const { return m_status; }
    void setStatus(const QString &f_status);

  Q_SIGNALS:
    // One signal per area-update type the protocol knows; the broadcaster
    // subscribes to exactly these four.
    void playerCountChanged(int f_count);
    void statusChanged(const QString &f_status);
    void ownersChanged();
    void lockStateChanged(LockState f_state);

  private:
    int m_id;
    QString m_name;
    int m_floor_id;
    int m_x;
    QSet<int> m_visible_areas;
    QVector<int> m_players;
    QList<int> m_characters_taken;
    QList<int> m_owners;
    QList<int> m_invited;
    LockState m_lock_state = LockState::Free;
    QString m_status = QStringLiteral("IDLE");
};

} // namespace akashi

