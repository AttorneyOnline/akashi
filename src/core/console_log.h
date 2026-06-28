#pragma once

#include "akashi_core_export.h"

namespace akashi {

// Installs the console message handler: aligned time, level and category
// columns, colored when the output is a live terminal. Redirected output
// (log files, pipes) stays plain text.
AKASHI_CORE_EXPORT void installConsoleLog();

} // namespace akashi
