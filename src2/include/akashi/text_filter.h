#ifndef AKASHI_TEXT_FILTER_H
#define AKASHI_TEXT_FILTER_H

#include <QString>

#include <functional>
#include <optional>

namespace akashi {

using TextFilterFn = std::function<std::optional<QString>(const QString &text)>;

} // namespace akashi

#endif // AKASHI_TEXT_FILTER_H
