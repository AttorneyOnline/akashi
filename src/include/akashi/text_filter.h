#pragma once

#include <QString>

#include <functional>
#include <optional>

namespace akashi {

using TextFilterFn = std::function<std::optional<QString>(const QString &text)>;

} // namespace akashi
