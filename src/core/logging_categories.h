#pragma once

#include "akashi_core_export.h"

#include <QLoggingCategory>

// The logging categories console messages file under, so owners can turn
// whole subsystems up or down without touching code. Qt reads the rules
// from the QT_LOGGING_RULES environment variable, for example:
//   QT_LOGGING_RULES="akashi.scripting.info=false;akashi.plugins.debug=true"
AKASHI_CORE_EXPORT const QLoggingCategory &akashiPlugins();
AKASHI_CORE_EXPORT const QLoggingCategory &akashiScripting();
