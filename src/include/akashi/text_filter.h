#pragma once

#include <QString>

#include <functional>
#include <optional>

namespace akashi {

using TextFilterFn = std::function<std::optional<QString>(const QString &text)>;

// Appended: this SDK header is append-only.

// Which chat surface a text travels on. Filters declare the channels they
// run on when they register; sanction filters stay IC-only by default.
enum class TextChannel
{
    Ic,
    Ooc,
};

// Who is speaking, for filters that need the actor - the moderation bot's
// pre-echo screen ties a dropped message to its sender this way. Filters
// registered with the plain text-only shape keep working unchanged.
struct TextFilterContext
{
    int client_session_id = -1;
    int player_state_id = -1;
    QString ipid;
    TextChannel channel = TextChannel::Ic;
};

using ContextTextFilterFn = std::function<std::optional<QString>(const QString &text, const TextFilterContext &context)>;

} // namespace akashi
