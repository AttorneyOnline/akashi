#pragma once

#include "akashi_core_export.h"
#include "proto/client_profile.h"
#include "proto/message.h"

#include <QString>

namespace akashi {

// HI: the client announces its hardware id.
class AKASHI_CORE_EXPORT HelloMessage : public Message
{
  public:
    QString hwid;
};

// ID: the client names its software and version. version_valid is false when
// the version field carried no parseable X.Y.Z - the only thing the handshake
// rejects, so any release (AO2, DRO, ...) that parses is accepted.
class AKASHI_CORE_EXPORT IdentifyMessage : public Message
{
  public:
    QString arch;
    ClientVersion version;
    bool version_valid = false;
};

// CC: the client picks a character; the spectator id -1 means none.
class AKASHI_CORE_EXPORT CharacterSelectMessage : public Message
{
  public:
    int char_id = -1;
};

// PW: the password the client offers for a protected character.
class AKASHI_CORE_EXPORT CharacterPasswordMessage : public Message
{
  public:
    QString password;
};

} // namespace akashi
