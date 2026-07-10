#pragma once

#include "akashi_core_export.h"

#include <QObject>
#include <QString>

namespace akashi {

class ClientSession;

// One user slot of a connection: another player-list presence of the same
// client, like a second window of the same person, wearing whatever
// character it likes. A legacy-protocol session always owns exactly one;
// a richer protocol may own several. Everything here is per-slot -
// the connection, identity, auth, sanctions and receive-preferences live
// on ClientSession, which owns the PlayerState(s).
//
// This is the unit the player list knows: PR/PU packets are keyed by id(),
// and the fields they carry are observable through the signals below - so
// each slot a person holds is its own entry in everyone's player list.
class AKASHI_CORE_EXPORT PlayerState : public QObject
{
    Q_OBJECT

  public:
    // The id is the player-list key. The session's first slot reuses the
    // session id, which keeps the network traffic identical for one-slot
    // clients; extra slots get their own ids when multi-slot support lands.
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
    // The last pair request that went out with a valid message; cleared
    // when the sender changes character or area.
    int pairing_with = -1;
    bool first_person = false;

    // The character's last IC line, for doublepost detection.
    QString last_message;

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
