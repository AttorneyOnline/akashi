#include "world/testimony_recorder.h"

#include "proto/ic.h"
#include "proto/packet.h"

namespace akashi {

Statement::Statement(const QStringList &f_ic_fields) :
    m_ic_fields(f_ic_fields)
{
}

std::optional<Statement> Statement::fromSavedLine(const QString &f_line)
{
    QStringList l_fields = f_line.split("#");
    for (QString &l_field : l_fields) {
        l_field = Packet::unescape(l_field);
    }
    if (l_fields.size() < 15) {
        return std::nullopt;
    }
    ICMessage l_ic = icMessageFromOutgoingFields(l_fields);
    l_ic.char_id = -1;
    return Statement(Ao2IcCodec().encode(l_ic).fields());
}

QString Statement::toSavedLine() const
{
    QStringList l_fields = m_ic_fields;
    for (QString &l_field : l_fields) {
        l_field = Packet::escape(l_field);
    }
    return l_fields.join("#");
}

void Statement::padTo(int f_size)
{
    while (m_ic_fields.size() < f_size) {
        m_ic_fields.append("");
    }
}

void Statement::setMessage(const QString &f_message)
{
    padTo(5);
    m_ic_fields[4] = f_message;
}

void Statement::setTextColor(const QString &f_color)
{
    padTo(15);
    m_ic_fields[14] = f_color;
}

QStringList Statement::playbackFields() const
{
    Statement l_playback = *this;
    l_playback.padTo(15);
    // -1 so no client mistakes the replay for its own message; see the
    // header for the whole story.
    l_playback.m_ic_fields[8] = "-1";
    if (l_playback.m_ic_fields.size() > 15 && l_playback.m_ic_fields[15].isEmpty()) {
        l_playback.m_ic_fields[15] = l_playback.m_ic_fields.value(2);
    }
    return l_playback.m_ic_fields;
}

std::optional<Statement> TestimonyRecorder::statementAt(int f_position) const
{
    if (f_position < 0 || f_position >= m_statements.size()) {
        return std::nullopt;
    }
    return m_statements.at(f_position);
}

void TestimonyRecorder::record(const Statement &f_statement)
{
    ++m_index;
    m_statements.append(f_statement);
}

void TestimonyRecorder::insert(int f_position, const Statement &f_statement)
{
    if (f_position >= 0 && f_position <= m_statements.size()) {
        m_statements.insert(f_position, f_statement);
    }
}

void TestimonyRecorder::replace(int f_position, const Statement &f_statement)
{
    if (f_position >= 0 && f_position < m_statements.size()) {
        m_statements.replace(f_position, f_statement);
    }
}

void TestimonyRecorder::remove(int f_position)
{
    if (f_position >= 0 && f_position < m_statements.size()) {
        m_statements.remove(f_position);
        --m_index;
    }
}

void TestimonyRecorder::clear()
{
    m_state = State::Stopped;
    m_index = -1;
    m_statements.clear();
}

void TestimonyRecorder::restart()
{
    m_state = State::Playback;
    m_index = 0;
}

std::optional<QPair<Statement, TestimonyRecorder::Playback>> TestimonyRecorder::jumpTo(int f_position)
{
    // A title alone is nothing to play back.
    if (m_statements.size() < 2) {
        return std::nullopt;
    }

    m_index = f_position;
    if (m_index > m_statements.size() - 1) {
        m_index = 1;
        return QPair<Statement, Playback>{m_statements.at(m_index), Playback::Looped};
    }
    if (m_index <= 1) {
        m_index = 1;
        return QPair<Statement, Playback>{m_statements.at(m_index), Playback::StayedAtFirst};
    }
    return QPair<Statement, Playback>{m_statements.at(m_index), Playback::Ok};
}

} // namespace akashi
