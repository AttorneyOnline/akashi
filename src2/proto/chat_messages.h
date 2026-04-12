#ifndef PROTO_CHAT_MESSAGES_H
#define PROTO_CHAT_MESSAGES_H

#include "akashi_core_export.h"
#include "proto/message.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace akashi {

// CT: an out-of-character chat line, or a command when it starts with a slash.
class AKASHI_CORE_EXPORT OocMessage : public Message
{
  public:
    QString name;
    QString message;
};

// DE: the client asks to delete one piece of evidence. No index means the
// client sent something that is not a number.
class AKASHI_CORE_EXPORT EvidenceDeleteMessage : public Message
{
  public:
    std::optional<int> index;
};

// EE: the client rewrites one piece of evidence.
class AKASHI_CORE_EXPORT EvidenceEditMessage : public Message
{
  public:
    int index = -1;
    QString name;
    QString description;
    QString image;
};

// SETCASE: the five casing roles a client wants alerts for. The list is
// empty when the client sent values that are not numbers.
class AKASHI_CORE_EXPORT CasingPreferencesMessage : public Message
{
  public:
    QList<bool> preferences;
};

// CASEA: a case announcement naming the five roles it still needs. The
// needs list is empty when the client sent values that are not numbers;
// the raw values go back out in the alert.
class AKASHI_CORE_EXPORT CaseAnnouncementMessage : public Message
{
  public:
    QString title;
    QList<bool> needs;
    QStringList needs_raw;
};

} // namespace akashi

#endif // PROTO_CHAT_MESSAGES_H
