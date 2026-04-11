#ifndef PROTO_AREA_MUSIC_MESSAGES_H
#define PROTO_AREA_MUSIC_MESSAGES_H

#include "akashi_core_export.h"
#include "proto/message.h"

#include <QString>

namespace akashi {

// MC: a music change, or an area move when the argument names an area
// instead of a song. The server fills the reply-only fields.
class AKASHI_CORE_EXPORT MusicChangeMessage : public Message
{
  public:
    QString argument;
    QString char_id;
    QString effects;
    bool has_effects = false;

    // Reply-only fields.
    QString showname;
    bool looping = true;
    int channel = 0;
};

// RT: a judge's witness-testimony or cross-examination splash; rulings
// carry which verdict was picked.
class AKASHI_CORE_EXPORT JudgeSplashMessage : public Message
{
  public:
    QString splash;
    QString variant;
    bool has_variant = false;
};

// PE: the client adds one piece of evidence.
class AKASHI_CORE_EXPORT EvidenceAddMessage : public Message
{
  public:
    QString name;
    QString description;
    QString image;
};

} // namespace akashi

#endif // PROTO_AREA_MUSIC_MESSAGES_H
