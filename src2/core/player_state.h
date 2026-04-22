#ifndef CORE_PLAYER_STATE_H
#define CORE_PLAYER_STATE_H

#include "akashi_core_export.h"

#include <QObject>
#include <QString>

namespace akashi {

class ClientSession;

// One playable character within a connection. A legacy (positional-wire)
// session always owns exactly one of these; a richer protocol may own
// several, letting one person voice multiple characters at once. Everything
// here is per-character - the connection, identity, auth, sanctions and
// receive-preferences live on ClientSession, which owns the PlayerState(s).
//
// This is the unit the player list knows: PR/PU packets are keyed by id(),
// and the fields they carry are observable through the signals below - so
// each character a person plays is its own entry in everyone's player list.
class AKASHI_CORE_EXPORT PlayerState : public QObject
{
    Q_OBJECT

  public:
    // The id is the player-list key. The session's first character reuses the
    // session id, which keeps the wire identical for one-character clients;
    // extra characters get their own ids when the multi-character wire lands.
    PlayerState(int f_id, ClientSession *f_session);

    int id() const { return m_id; }

    // The connection this character belongs to - where its packets go.
    ClientSession *session() const { return m_session; }

    // The player-list fields. Mutations go through the setters so the list
    // hears about them; each setter only reports an actual change.
    QString oocName() const { return m_ooc_name; }
    void setOocName(const QString &f_name);

    QString character() const { return m_character; }
    void setCharacter(const QString &f_character);

    QString showname() const { return m_showname; }
    void setShowname(const QString &f_showname);

    int areaId() const { return m_area_id; }
    void setAreaId(int f_area_id);

    // Character identity beyond the player list.
    int char_id = -1;
    QString iniswap; // folder actually shown, differs when iniswapped
    bool spectator = true;

    // How the character presents.
    QString pos;
    QString offset;
    QString flipping;
    QString emote;
    int pairing_with = -1;
    bool first_person = false;

    // The character's last IC line, for doublepost detection.
    QString last_message;

    // A per-character medieval-speak curse (the area can also force it).
    bool medieval = false;

  Q_SIGNALS:
    void nameChanged(const QString &f_name);
    void characterChanged(const QString &f_character);
    void shownameChanged(const QString &f_showname);
    void areaIdChanged(int f_area_id);

  private:
    int m_id;
    ClientSession *m_session;
    QString m_ooc_name;
    QString m_character; // internal character folder name
    QString m_showname;  // custom in-character showname
    int m_area_id = 0;
};

} // namespace akashi

#endif // CORE_PLAYER_STATE_H
