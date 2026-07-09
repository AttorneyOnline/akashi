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

} // namespace akashi
