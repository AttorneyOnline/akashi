#ifndef WORLD_TESTIMONY_RECORDER_H
#define WORLD_TESTIMONY_RECORDER_H

#include "akashi_core_export.h"

#include <QPair>
#include <QStringList>
#include <QVector>

#include <optional>

namespace akashi {

// One recorded statement: the outgoing IC message it was made from, with
// the spots the recorder works on named instead of numbered. The accessors
// are index-safe, so a malformed statement from a saved file reads as
// empty values instead of crashing anything.
class AKASHI_CORE_EXPORT Statement
{
  public:
    Statement() = default;
    explicit Statement(const QStringList &f_ic_fields);

    // Reads one line of a saved testimony file. The fields go through the
    // same IC message parsing a live client packet does, so a hand-edited
    // or truncated line comes out as a well-formed statement or as nothing,
    // never as a half-parsed one. The char id is cleared to -1: the saved
    // id belonged to a player slot in a long-gone session, and replaying it
    // could name whoever holds that slot now.
    static std::optional<Statement> fromSavedLine(const QString &f_line);

    // The form fromSavedLine reads back: fields wire-escaped and joined
    // with #, so a message containing a literal # cannot break the line
    // apart. Old unescaped files still load, since unescaping text that
    // was never escaped changes nothing.
    QString toSavedLine() const;

    QStringList icFields() const { return m_ic_fields; }

    QString message() const { return m_ic_fields.value(4); }
    void setMessage(const QString &f_message);

    QString side() const { return m_ic_fields.value(5); }
    int charId() const { return m_ic_fields.value(8).toInt(); }

    QString textColor() const { return m_ic_fields.value(14); }
    void setTextColor(const QString &f_color);

    // The fields to broadcast when replaying. Nobody owns a replayed
    // statement: a client that receives its own char id treats the message
    // as its own echo and clears the player's input panel mid-typing, so
    // the char id becomes -1, which belongs to no one and every client
    // accepts. With no char id the client shows the showname field, so an
    // empty showname is filled with the character folder name to keep the
    // name box readable.
    QStringList playbackFields() const;

  private:
    void padTo(int f_size);

    QStringList m_ic_fields;
};

// The testimony of one area: a title followed by recorded statements that
// can be played back, stepped through, edited and saved. This is the real
// class behind what used to be raw string lists indexed by magic numbers.
class AKASHI_CORE_EXPORT TestimonyRecorder
{
  public:
    enum class State
    {
        Stopped,
        Recording,
        Update,
        Add,
        Playback,
    };

    // How a playback jump ended up where it did.
    enum class Playback
    {
        Ok,
        Looped,
        StayedAtFirst,
    };

    State state() const { return m_state; }
    void setState(State f_state) { m_state = f_state; }

    // Where recording or playback currently stands: -1 before the title,
    // 0 on the title, 1 and up on the statements.
    int statementIndex() const { return m_index; }
    int statementCount() const { return m_statements.size(); }

    std::optional<Statement> statementAt(int f_position) const;

    // Appends while recording and advances the index.
    void record(const Statement &f_statement);

    // The editing operations ignore positions that do not exist.
    void insert(int f_position, const Statement &f_statement);
    void replace(int f_position, const Statement &f_statement);
    void remove(int f_position);

    // Forgets everything and stops.
    void clear();

    // Starts playback over from the title.
    void restart();

    // Moves playback to the statement and returns it: past the last
    // statement loops back to the first, at or before the first stays
    // there. A testimony without statements has nothing to play and
    // returns nothing - the old code crashed here.
    std::optional<QPair<Statement, Playback>> jumpTo(int f_position);

  private:
    QVector<Statement> m_statements;
    State m_state = State::Stopped;
    int m_index = -1;
};

} // namespace akashi

#endif // WORLD_TESTIMONY_RECORDER_H
