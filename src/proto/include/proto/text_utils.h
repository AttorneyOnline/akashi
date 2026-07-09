#pragma once

#include "akashi_core_export.h"

#include <QString>

namespace akashi {

// Removes the combining characters used for zalgo text.
AKASHI_CORE_EXPORT QString stripZalgo(QString f_text);

// Strips path-traversal sequences from a client-supplied position. The one
// sanitizer for every position write and every position echo.
AKASHI_CORE_EXPORT QString sanitizePosition(QString f_position);

} // namespace akashi
