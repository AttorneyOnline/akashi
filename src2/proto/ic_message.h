#pragma once

#include "akashi_core_export.h"
#include "proto/message.h"

#include <QString>

namespace akashi {

// MS: one in-character message carrying the full flat data model.
// Numbers hold what the protocol coerced them to; free-form fields
// stay untouched strings so every byte survives the round trip.
class AKASHI_CORE_EXPORT ICMessage : public Message
{
  public:
    // The classic fifteen.
    QString desk_mod;
    QString preanim;
    QString char_name;
    QString emote;
    QString message_text;
    QString side;
    QString sfx_name;
    int emote_mod = 0;
    int char_id = -1;
    int sfx_delay = 0;
    QString objection_mod;
    int evidence = 0;
    int flip = 0;
    int realization = 0;
    int text_color = 0;

    // 2.6 extensions. The pair request comes in; the resolved pair goes out.
    QString showname;
    int pair_requested = -1;
    QString pair_front_back;
    int other_char_id = -1;
    QString other_name;
    QString other_emote;
    QString self_offset;
    QString other_offset;
    QString other_flip;
    int immediate = 0;

    // 2.8 extensions.
    int sfx_looping = 0;
    int screenshake = 0;
    QString frames_shake;
    QString frames_realization;
    QString frames_sfx;
    int additive = 0;
    QString effect;

    // Trailing extras.
    QString blips;
    QString slide;

    // Which extension tiers the sender's packet carried, so the reply
    // keeps exactly the field count the room expects.
    bool has_pair_data = false;
    bool has_effect_data = false;
    bool has_blips = false;
    bool has_slide = false;
};

} // namespace akashi

