#ifndef CORE_PLAYER_STATE_H
#define CORE_PLAYER_STATE_H

#include "akashi_core_export.h"

#include <QString>

namespace akashi {

// One playable character within a connection. A legacy (positional-wire)
// session always owns exactly one of these; a richer protocol may own
// several, letting one person voice multiple characters at once. Everything
// here is per-character - the connection, identity, auth, sanctions and
// receive-preferences live on ClientSession, which owns the PlayerState(s).
class AKASHI_CORE_EXPORT PlayerState
{
  public:
    // Character identity, shown in the player list.
    int char_id = -1;
    QString character; // internal character folder name
    QString iniswap;   // folder actually shown, differs when iniswapped
    QString ooc_name;  // out-of-character nickname
    QString showname;  // custom in-character showname
    bool spectator = true;

    // Where the character is and how it presents.
    int area_id = 0;
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
};

} // namespace akashi

#endif // CORE_PLAYER_STATE_H
