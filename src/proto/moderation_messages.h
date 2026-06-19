#pragma once

#include "akashi_core_export.h"
#include "proto/message.h"

#include <QString>

namespace akashi {

// ZZ: a player calls for a moderator, optionally about another player.
class AKASHI_CORE_EXPORT ModcallMessage : public Message
{
  public:
    QString reason;
    int target_id = -1;
};

// MA: a moderator kicks (duration 0) or bans a player from the modcall
// dialog. A duration of -1 bans permanently.
class AKASHI_CORE_EXPORT ModActionMessage : public Message
{
  public:
    int target_id = -1;
    int duration = 0;
    QString reason;
};

} // namespace akashi

