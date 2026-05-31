#pragma once

#include "akashi_core_export.h"

#include <QString>

namespace akashi {

// Removes the combining characters used for zalgo text.
AKASHI_CORE_EXPORT QString stripZalgo(QString f_text);

} // namespace akashi

